#!/usr/bin/env python3
"""Dependency-free OPC UA smoke test for the ESP32 OPC UA Server.

Speaks raw OPC UA binary over TCP with the standard library only - no
opcua/asyncua packages needed. It runs the same handshake a real client
performs:

  HEL/ACK -> OpenSecureChannel (policy None) -> GetEndpoints ->
  CreateSession -> ActivateSession (anonymous) -> Read all tags ->
  Write Setpoint -> CloseSession

Usage:
    python tools/opcua_test.py                       # opc.tcp://192.168.1.50:4840
    python tools/opcua_test.py --host 10.0.0.5 --port 4840
    python tools/opcua_test.py --skip-write
"""

import argparse
import socket
import struct
import sys
import time
import random

POLICY_NONE = "http://opcfoundation.org/UA/SecurityPolicy#None"

# Service request/response Encoding_DefaultBinary node ids (ns=0)
SVC = {
    "get_endpoints": 428,
    "create_session": 461,
    "activate_session": 467,
    "close_session": 473,
    "read": 631,
    "write": 673,
    "open_channel": 446,
}

# Builtin variant type ids (1-based, per OPC UA part 6)
VT_BOOLEAN, VT_STRING, VT_DATETIME, VT_DOUBLE = 1, 12, 13, 11

TAGS = [
    ("DeviceName", VT_STRING),
    ("Temperature", VT_DOUBLE),
    ("Uptime", VT_DOUBLE),
    ("FreeHeap", VT_DOUBLE),
    ("WiFiRSSI", VT_DOUBLE),
    ("Sine", VT_DOUBLE),
    ("Setpoint", VT_DOUBLE),
    ("RelayOutput", VT_BOOLEAN),
    ("ButtonInput", VT_BOOLEAN),
]


class EncodeError(Exception):
    pass


# --------------------------------------------------------------------------
# Writer / Reader for the OPC UA little-endian binary encoding
# --------------------------------------------------------------------------

class W:
    def __init__(self):
        self.b = bytearray()

    def u8(self, v):
        self.b += struct.pack("<B", v)

    def u16(self, v):
        self.b += struct.pack("<H", v)

    def u32(self, v):
        self.b += struct.pack("<I", v)

    def i32(self, v):
        self.b += struct.pack("<i", v)

    def i64(self, v):
        self.b += struct.pack("<q", v)

    def f64(self, v):
        self.b += struct.pack("<d", v)

    def boolean(self, v):
        self.u8(1 if v else 0)

    def string(self, text):
        if text is None:
            self.i32(-1)
        else:
            raw = text.encode("utf-8")
            self.i32(len(raw))
            self.b += raw

    def bytestring(self, raw):
        if raw is None:
            self.i32(-1)
        else:
            self.i32(len(raw))
            self.b += raw

    def array_len(self, count):
        self.i32(count)

    def nodeid(self, ns, ident):
        """Numeric identifier unless `ident` is a str (string identifier)."""
        if isinstance(ident, str):
            self.u8(0x03)
            self.u16(ns)
            self.string(ident)
        elif ns < 256 and ident <= 0xFF:
            self.u8(0x00)
            self.u8(ident)
        elif ns < 256 and ident <= 0xFFFF:
            self.u8(0x01)
            self.u8(ns)
            self.u16(ident)
        else:
            self.u8(0x02)
            self.u16(ns)
            self.u32(ident)

    def localized_text(self, locale, text):
        mask = (0x01 if locale is not None else 0) | (0x02 if text is not None else 0)
        self.u8(mask)
        if locale is not None:
            self.string(locale)
        if text is not None:
            self.string(text)

    def qualified_name(self, ns, name):
        self.u16(ns)
        self.string(name)

    def extobj_null(self):
        self.nodeid(0, 0)
        self.u8(0x00)

    def variant(self, vtype, value):
        if value is None:
            self.u8(0)
            return
        self.u8(vtype)
        if vtype == VT_BOOLEAN:
            self.boolean(value)
        elif vtype == VT_DOUBLE:
            self.f64(value)
        elif vtype == VT_STRING:
            self.string(value)
        else:
            raise EncodeError("unsupported variant type %d" % vtype)

    def datavalue(self, vtype, value):
        self.u8(0x01)   # mask: value present
        self.variant(vtype, value)

    def request_header(self, token, handle):
        self.nodeid(*token)          # authenticationToken
        self.i64(0)                  # timestamp
        self.u32(handle)             # requestHandle
        self.u32(0)                  # returnDiagnostics
        self.string(None)            # auditEntryId
        self.u32(10000)              # timeoutHint
        self.extobj_null()           # additionalHeader

    def application_description(self, app_uri, app_name):
        self.string(app_uri)
        self.string(app_uri)
        self.localized_text(None, app_name)
        self.i32(1)                  # applicationType: Client
        self.string(None)            # gatewayServerUri
        self.string(None)            # discoveryProfileUri
        self.array_len(-1)           # discoveryUrls


class R:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def take(self, n):
        if self.p + n > len(self.d):
            raise EncodeError("decode past end (at %d, need %d, have %d)"
                              % (self.p, n, len(self.d)))
        raw = self.d[self.p:self.p + n]
        self.p += n
        return raw

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return struct.unpack("<H", self.take(2))[0]

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def i32(self):
        return struct.unpack("<i", self.take(4))[0]

    def i64(self):
        return struct.unpack("<q", self.take(8))[0]

    def f64(self):
        return struct.unpack("<d", self.take(8))[0]

    def boolean(self):
        return self.u8() != 0

    def string(self):
        n = self.i32()
        if n < 0:
            return None
        return self.take(n).decode("utf-8", "replace")

    def bytestring(self):
        n = self.i32()
        if n < 0:
            return None
        return self.take(n)

    def nodeid(self):
        kind = self.u8()
        if kind == 0x00:
            return 0, self.u8()
        if kind == 0x01:
            ns = self.u8()
            return ns, self.u16()
        if kind == 0x02:
            return self.u16(), self.u32()
        if kind == 0x03:
            return self.u16(), self.string()
        raise EncodeError("unsupported NodeId encoding 0x%02X" % kind)

    def localized_text(self):
        mask = self.u8()
        locale = self.string() if mask & 0x01 else None
        text = self.string() if mask & 0x02 else None
        return locale, text

    def qualified_name(self):
        return self.u16(), self.string()

    def extobj(self):
        self.nodeid()
        mask = self.u8()
        if mask & 0x01:
            return self.bytestring()
        if mask & 0x02:
            self.string()
        return None

    def variant(self):
        t = self.u8()
        if t == 0:
            return None, 0
        dims = t & 0x40
        arr = t & 0x80
        t = t & 0x3F
        if arr:
            n = self.i32()
            vals = [self._scalar(t) for _ in range(max(n, 0))]
            if dims:
                ndim = self.i32()
                for _ in range(max(ndim, 0)):
                    self.i32()
            return vals, t
        return self._scalar(t), t

    def _scalar(self, t):
        if t == VT_BOOLEAN:
            return self.boolean()
        if t == VT_DOUBLE:
            return self.f64()
        if t == VT_STRING:
            return self.string()
        if t == VT_DATETIME:
            return self.i64()
        if t in (2, 3, 4, 5):
            return {"2": self.i32, "3": self.i32, "4": self.i32, "5": self.u32}[str(t)]() \
                if False else (self.i32() if t in (2, 4) else self.u32())
        if t in (6, 7):
            return self.i32() if t == 6 else self.u32()
        if t in (8, 9):
            return self.i64() if t == 8 else self.u64()
        if t == 10:
            return self.f64()
        if t == 11:
            return self.f64()
        raise EncodeError("unsupported scalar type %d" % t)

    def u64(self):
        return struct.unpack("<Q", self.take(8))[0]

    def datavalue(self):
        mask = self.u8()
        value = stype = None
        status = 0
        src_ts = srv_ts = 0
        if mask & 0x01:
            value, stype = self.variant()
        if mask & 0x02:
            status = self.u32()
        if mask & 0x04:
            src_ts = self.i64()
        if mask & 0x08:
            srv_ts = self.i64()
        if mask & 0x10:
            self.u16()   # sourcePicoseconds
        if mask & 0x20:
            self.u16()   # serverPicoseconds
        return value, stype, status, src_ts, srv_ts

    def diagnostic_info(self):
        """Recursive DiagnosticInfo; returns for null (mask 0x00)."""
        mask = self.u8()
        if mask & 0x01:
            self.i32()   # symbolicId
        if mask & 0x02:
            self.i32()   # namespaceUri
        if mask & 0x04:
            self.i32()   # localizedText
        if mask & 0x08:
            self.i32()   # locale
        if mask & 0x10:
            self.string()   # additionalInfo
        if mask & 0x20:
            self.u32()   # innerStatusCode
        if mask & 0x40:
            self.diagnostic_info()   # innerDiagnosticInfo

    def response_header(self):
        """Decodes a service ResponseHeader, returns its serviceResult."""
        self.i64()       # timestamp
        self.u32()       # requestHandle
        status = self.u32()
        self.diagnostic_info()   # serviceDiagnostics
        n = self.i32()   # stringTable
        for _ in range(max(n, 0)):
            self.string()
        self.extobj()    # additionalHeader
        return status

    def endpoint_description(self):
        url = self.string()
        # ApplicationDescription
        self.string()            # applicationUri
        self.string()            # productUri
        self.localized_text()    # applicationName
        self.i32()               # applicationType
        self.string()            # gatewayServerUri
        self.string()            # discoveryProfileUri
        n = self.i32()           # discoveryUrls
        for _ in range(max(n, 0)):
            self.string()
        self.bytestring()        # serverCertificate
        mode = self.i32()        # securityMode
        policy = self.string()   # securityPolicyUri
        tokens = []
        n = self.i32()           # userIdentityTokens
        for _ in range(max(n, 0)):
            pid = self.string()
            self.i32()           # tokenType
            self.string()        # issuedTokenType
            self.string()        # issuerEndpointUrl
            self.string()        # securityPolicyUri
            tokens.append((pid, 0))
        self.string()            # transportProfileUri
        self.u8()                # securityLevel
        return {"url": url, "mode": mode, "policy": policy, "tokens": tokens}


# --------------------------------------------------------------------------
# Transport
# --------------------------------------------------------------------------

class Client:
    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port), timeout=6)
        self.channel_id = 0
        self.token_id = 0
        self.seq = random.randint(1, 100)
        self.req_id = random.randint(1, 100)
        self.handle = random.randint(1, 100)
        self.token = (0, 0)
        self.endpoints = []

    def _send(self, kind, body):
        size = 8 + len(body)
        self.sock.sendall(kind + struct.pack("<I", size) + body)

    def _recv(self):
        header = self._recv_exact(8)
        kind = header[:4]
        size = struct.unpack("<I", header[4:])[0]
        return kind, self._recv_exact(size - 8)

    def _recv_exact(self, n):
        data = b""
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                raise EncodeError("connection closed early")
            data += chunk
        return data

    def _next_seq(self):
        self.seq += 1
        self.req_id += 1
        self.handle += 1
        return self.seq, self.req_id, self.handle

    # --- handshake -------------------------------------------------------

    def hello(self):
        w = W()
        w.u32(0)          # protocolVersion
        w.u32(16777216)   # receiveBufferSize
        w.u32(16777216)   # sendBufferSize
        w.u32(0)          # maxMessageSize (no limit)
        w.u32(0)          # maxChunkCount (no limit)
        body = b"HELF" + struct.pack("<I", 8 + len(w.b)) + w.b
        self.sock.sendall(body)
        kind, payload = self._recv()
        if kind != b"ACKF":
            raise EncodeError("expected ACKF, got %r" % kind)
        r = R(payload)
        return {"protocolVersion": r.u32(), "receive": r.u32(),
                "send": r.u32(), "maxMessage": r.u32(), "maxChunk": r.u32()}

    def open_channel(self):
        seq, req_id, handle = self._next_seq()
        w = W()
        w.u32(0)                 # secureChannelId (0 = new)
        w.string(POLICY_NONE)
        w.bytestring(None)       # senderCertificate
        w.bytestring(None)       # receiverCertificateThumbprint
        w.u32(seq)
        w.u32(req_id)
        w.nodeid(0, SVC["open_channel"])
        w.request_header((0, 0), handle)
        w.u32(0)                 # clientProtocolVersion
        w.i32(0)                 # requestType: ISSUE
        w.i32(1)                 # securityMode: NONE
        w.bytestring(None)       # clientNonce
        w.u32(60000)             # requestedLifetime
        self._send(b"OPNF", w.b)

        kind, payload = self._recv()
        if kind != b"OPNF":
            raise EncodeError("expected OPNF response, got %r" % kind)
        r = R(payload)
        self.channel_id = r.u32()
        r.string()               # securityPolicyUri
        r.bytestring()           # senderCertificate
        r.bytestring()           # receiverCertificateThumbprint
        r.u32()                  # sequenceNumber
        r.u32()                  # requestId
        r.nodeid()               # OpenSecureChannelResponse
        status = r.response_header()
        if status != 0:
            raise EncodeError("OpenSecureChannel failed: 0x%08X" % status)
        r.u32()                  # serverProtocolVersion
        self.token_id = r.u32()  # securityToken.channelId
        r.u32()                  # securityToken.tokenId
        r.i64()                  # securityToken.createdAt
        r.u32()                  # securityToken.revisedLifetime
        r.bytestring()           # serverNonce
        return self.channel_id

    def _service(self, service_id, build_body):
        seq, req_id, handle = self._next_seq()
        w = W()
        w.u32(self.channel_id)
        w.u32(self.token_id)
        w.u32(seq)
        w.u32(req_id)
        w.nodeid(0, service_id)
        build_body(w, handle)
        self._send(b"MSGF", w.b)
        kind, payload = self._recv()
        if kind != b"MSGF":
            raise EncodeError("expected MSGF response, got %r" % kind)
        r = R(payload)
        r.u32()                  # channelId
        r.u32()                  # tokenId
        r.u32()                  # sequenceNumber
        r.u32()                  # requestId
        node = r.nodeid()
        status = r.response_header()
        return r, node, status

    def get_endpoints(self, url):
        def build(w, handle):
            w.request_header(self.token, handle)
            w.string(url)
            w.array_len(-1)      # localeIds
            w.array_len(-1)      # profileUris

        r, node, status = self._service(SVC["get_endpoints"], build)
        if node[1] != SVC["get_endpoints"] + 1:
            raise EncodeError("unexpected GetEndpointsResponse node id %s" % (node,))
        n = r.i32()
        self.endpoints = [r.endpoint_description() for _ in range(max(n, 0))]
        r.array_len() if False else r.i32()   # diagnosticInfos
        return status, self.endpoints

    def create_session(self, url):
        nonce = bytes(random.getrandbits(8) for _ in range(32))

        def build(w, handle):
            w.request_header(self.token, handle)
            w.application_description("urn:py:opcua_test", "opcua_test")
            w.string(None)       # serverUri
            w.string(url)        # endpointUrl
            w.string("opcua_test session")
            w.bytestring(nonce)  # clientNonce
            w.bytestring(None)   # clientCertificate
            w.f64(120000.0)      # requestedSessionTimeout
            w.u32(0)             # maxResponseMessageSize

        r, node, status = self._service(SVC["create_session"], build)
        if node[1] != SVC["create_session"] + 1:
            raise EncodeError("unexpected CreateSessionResponse node id %s" % (node,))
        session_id = r.nodeid()
        self.token = r.nodeid()  # authenticationToken
        n = r.i32()              # serverEndpoints
        for _ in range(max(n, 0)):
            r.endpoint_description()
        n = r.i32()              # serverSoftwareCertificates
        for _ in range(max(n, 0)):
            r.bytestring()       # certificateData
            r.bytestring()       # signature
        r.string()               # serverSignature.algorithm
        r.bytestring()           # serverSignature.signature
        r.bytestring()           # serverNonce
        r.bytestring()           # serverCertificate
        r.u32()                  # maxRequestMessageSize
        return status, session_id

    def activate_session(self, anonymous_policy):
        def build(w, handle):
            w.request_header(self.token, handle)
            w.string(None)       # clientSignature.algorithm
            w.bytestring(None)   # clientSignature.signature
            w.array_len(-1)      # clientSoftwareCertificates
            w.array_len(-1)      # localeIds
            # userIdentityToken ExtensionObject: AnonymousIdentityToken
            w.nodeid(0, 321)     # AnonymousIdentityToken_Encoding_DefaultBinary
            w.u8(0x01)           # body as ByteString
            inner = W()
            inner.string(anonymous_policy)
            w.bytestring(bytes(inner.b))
            w.string(None)       # userTokenSignature.algorithm
            w.bytestring(None)   # userTokenSignature.signature

        r, node, status = self._service(SVC["activate_session"], build)
        if node[1] != SVC["activate_session"] + 1:
            raise EncodeError("unexpected ActivateSessionResponse node id %s" % (node,))
        n = r.i32()              # results
        codes = [r.u32() for _ in range(max(n, 0))]
        r.i32()                  # diagnosticInfos
        r.bytestring()           # serverNonce
        r.string()               # serverSignature.algorithm
        r.bytestring()           # serverSignature.signature
        return status, codes

    def read(self, nodes):
        def build(w, handle):
            w.request_header(self.token, handle)
            w.f64(0.0)           # maxAge
            w.i32(2)             # timestampsToReturn: Both
            w.array_len(len(nodes))
            for ns, ident in nodes:
                w.nodeid(ns, ident)
                w.u32(13)        # attributeId: Value
                w.string(None)   # indexRange
                w.qualified_name(0, None)   # dataEncoding

        r, node, status = self._service(SVC["read"], build)
        if node[1] != SVC["read"] + 1:
            raise EncodeError("unexpected ReadResponse node id %s" % (node,))
        n = r.i32()
        results = [r.datavalue() for _ in range(max(n, 0))]
        r.i32()                  # diagnosticInfos
        return status, results

    def write(self, ns, ident, vtype, value):
        def build(w, handle):
            w.request_header(self.token, handle)
            w.array_len(1)
            w.nodeid(ns, ident)
            w.u32(13)            # attributeId: Value
            w.string(None)       # indexRange
            w.datavalue(vtype, value)

        r, node, status = self._service(SVC["write"], build)
        if node[1] != SVC["write"] + 1:
            raise EncodeError("unexpected WriteResponse node id %s" % (node,))
        n = r.i32()
        codes = [r.u32() for _ in range(max(n, 0))]
        r.i32()                  # diagnosticInfos
        return status, codes

    def close_session(self):
        def build(w, handle):
            w.request_header(self.token, handle)
            w.boolean(True)      # deleteSubscriptions

        r, node, status = self._service(SVC["close_session"], build)
        return status


# --------------------------------------------------------------------------
# Test flow
# --------------------------------------------------------------------------

def fmt_variant(value, vtype):
    if vtype == VT_STRING:
        return repr(value)
    return str(value)


def main():
    ap = argparse.ArgumentParser(description="ESP32 OPC UA Server smoke test")
    ap.add_argument("--host", default="192.168.1.50")
    ap.add_argument("--port", type=int, default=4840)
    ap.add_argument("--skip-write", action="store_true",
                    help="do not test the Setpoint write")
    args = ap.parse_args()

    url = "opc.tcp://%s:%d" % (args.host, args.port)
    print("ESP32 OPC UA Server smoke test")
    print("Endpoint: %s" % url)
    print()

    checks = []

    def check(name, ok, detail=""):
        checks.append(ok)
        print("[%s] %-28s %s" % ("PASS" if ok else "FAIL", name, detail))

    client = Client(args.host, args.port)
    try:
        ack = client.hello()
        check("HEL/ACK", True,
              "buffers %d/%d" % (ack["receive"], ack["send"]))

        channel = client.open_channel()
        check("OpenSecureChannel", channel > 0, "channel id %d" % channel)

        status, endpoints = client.get_endpoints(url)
        none_eps = [e for e in endpoints if e["policy"] == POLICY_NONE]
        check("GetEndpoints", status == 0 and none_eps,
              "%d endpoint(s), %d with policy None" % (len(endpoints), len(none_eps)))
        anon = "anonymous"
        if none_eps:
            for pid, ttype in none_eps[0]["tokens"]:
                if ttype == 0:   # anonymous
                    anon = pid
                    break

        status, session_id = client.create_session(url)
        check("CreateSession", status == 0,
              "session id %s" % (session_id,))

        status, codes = client.activate_session(anon)
        check("ActivateSession (anonymous)", status == 0 and all(c == 0 for c in codes),
              "policy '%s'" % anon)

        read_nodes = [(1, name) for name, _ in TAGS]
        read_nodes.append((0, 2258))   # Server_ServerStatus_CurrentTime
        status, results = client.read(read_nodes)
        print()
        print("  Read results (attribute Value):")
        for (name, _), (value, vtype, st, src_ts, _srv) in zip(TAGS, results):
            if st == 0x80380000:
                # BadNodeIdUnknown: tag not implemented on this target
                # (e.g. the reference simulator lacks the ESP32-only tags)
                print("    ns=1;s=%-13s %-12s not present on this server" % (name, ""))
                continue
            ok = st == 0
            text = fmt_variant(value, vtype) if ok else "status 0x%08X" % st
            print("    ns=1;s=%-13s %-12s %s" % (name, "", text))
            check("Read ns=1;s=%s" % name, ok, "")
        if len(results) > len(TAGS):
            value, vtype, st, _, _ = results[-1]
            check("Read ns=0;i=2258", st == 0,
                  "server time ticks %s" % value if st == 0 else "")

        if not args.skip_write:
            old = None
            # Find current setpoint from the read above
            for (name, _), (value, vtype, st, _, _) in zip(TAGS, results):
                if name == "Setpoint" and st == 0 and vtype == VT_DOUBLE:
                    old = value
            new_value = round((old or 21.5) + 1.0, 3)
            status, codes = client.write(1, "Setpoint", VT_DOUBLE, new_value)
            check("Write ns=1;s=Setpoint", status == 0 and all(c == 0 for c in codes),
                  "wrote %.3f" % new_value)

            status, results = client.read([(1, "Setpoint")])
            value, vtype, st, _, _ = results[0]
            check("Read back Setpoint", st == 0 and abs(value - new_value) < 1e-6,
                  "now %.3f" % value if st == 0 else "")

        status = client.close_session()
        check("CloseSession", status == 0, "")
    except (EncodeError, socket.timeout, ConnectionError, OSError) as exc:
        check("transport", False, str(exc))
    finally:
        client.sock.close()

    passed = sum(1 for c in checks if c)
    print()
    print("Result: %d/%d checks passed" % (passed, len(checks)))
    sys.exit(0 if passed == len(checks) and checks else 1)


if __name__ == "__main__":
    main()
