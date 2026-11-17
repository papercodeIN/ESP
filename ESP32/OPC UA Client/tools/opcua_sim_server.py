#!/usr/bin/env python3
"""Minimal OPC UA reference server (stdlib only) for bench-testing the
ESP32 OPC UA Client without a second board or a real PLC.

Implements just enough of the protocol for the firmware's use case:

  HEL/ACK -> OpenSecureChannel (policy None) -> GetEndpoints ->
  CreateSession -> ActivateSession (anonymous or username) -> Read/Write ->
  CloseSession -> CloseSecureChannel

Simulated tags (namespace 1, string ids - matching the companion
"OPC UA Server" firmware):

    DeviceName   String   RO
    Temperature  Double   RO  (sine wave)
    Uptime       Double   RO
    Sine         Double   RO
    Setpoint     Double   RW  (writable, kept in memory)
    RelayOutput  Boolean  RW

plus the standard server clock ns=0;i=2258.

Usage:
    python tools/opcua_sim_server.py                       # port 4840
    python tools/opcua_sim_server.py --port 4840 --name DemoUnit
"""

import argparse
import math
import socket
import struct
import sys
import threading
import time

POLICY_NONE = "http://opcfoundation.org/UA/SecurityPolicy#None"

SVC_GET_ENDPOINTS = 428
SVC_CREATE_SESSION = 461
SVC_ACTIVATE_SESSION = 467
SVC_CLOSE_SESSION = 473
SVC_READ = 631
SVC_WRITE = 673
SVC_OPEN_CHANNEL = 446

ATTR_VALUE = 13

VT_BOOLEAN = 1
VT_STRING = 12
VT_DATETIME = 13
VT_DOUBLE = 11

BOOT_TIME = time.time()


class DecodeError(Exception):
    pass


# --------------------------------------------------------------------------
# Reader / Writer
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

    def nodeid(self, ns, ident):
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
        self.u8(vtype)
        if vtype == VT_BOOLEAN:
            self.boolean(value)
        elif vtype == VT_DOUBLE:
            self.f64(value)
        elif vtype == VT_STRING:
            self.string(value)
        elif vtype == VT_DATETIME:
            self.i64(value)
        else:
            raise DecodeError("unsupported variant type %d" % vtype)

    def datavalue(self, vtype, value):
        # mask: value + status + sourceTimestamp + serverTimestamp
        self.u8(0x01 | 0x02 | 0x04 | 0x08)
        self.variant(vtype, value)
        self.u32(0)                       # status Good
        self.i64(datetime_now_ticks())    # sourceTimestamp
        self.i64(datetime_now_ticks())    # serverTimestamp

    def array_str(self, items):
        if items is None:
            self.i32(-1)
            return
        self.i32(len(items))
        for item in items:
            self.string(item)

    def endpoint_description(self, url, device_name):
        self.string(url)
        self.string("urn:esp32:opcua:simulator")
        self.string("urn:esp32:opcua:simulator")
        self.localized_text("en", device_name)
        self.i32(0)                       # applicationType: Server
        self.string(None)                 # gatewayServerUri
        self.string(None)                 # discoveryProfileUri
        self.array_str([url])             # discoveryUrls
        self.bytestring(None)             # serverCertificate
        self.i32(1)                       # securityMode: NONE
        self.string(POLICY_NONE)
        self.i32(2)                       # userIdentityTokens
        for pid, ttype in (("anonymous", 0), ("username", 1)):
            self.string(pid)
            self.i32(ttype)
            self.string(None)
            self.string(None)
            self.string(None)
        self.string("http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary")
        self.u8(0)                        # securityLevel


class R:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def take(self, n):
        if self.p + n > len(self.d):
            raise DecodeError("decode past end")
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
            return self.u8(), self.u16()
        if kind == 0x02:
            return self.u16(), self.u32()
        if kind == 0x03:
            return self.u16(), self.string()
        raise DecodeError("unsupported NodeId encoding 0x%02X" % kind)

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
        arr = t & 0x80
        t = t & 0x3F
        if arr:
            n = self.i32()
            for _ in range(max(n, 0)):
                self.scalar(t)
            return None, t
        return self.scalar(t), t

    def scalar(self, t):
        if t == VT_BOOLEAN:
            return self.boolean()
        if t == VT_DOUBLE or t == 10:
            return self.f64()
        if t == VT_STRING:
            return self.string()
        if t == VT_DATETIME:
            return self.i64()
        if t in (2, 4, 6, 8):
            return self.i64() if t == 8 else self.i32()
        if t in (3, 5, 7, 9):
            return self.u64() if t == 9 else self.u32()
        raise DecodeError("unsupported scalar type %d" % t)

    def datavalue(self):
        mask = self.u8()
        value = None
        if mask & 0x01:
            value, _ = self.variant()
        if mask & 0x02:
            self.u32()
        if mask & 0x04:
            self.i64()
        if mask & 0x08:
            self.i64()
        if mask & 0x10:
            self.u16()
        if mask & 0x20:
            self.u16()
        return value

    def request_header(self):
        self.nodeid()          # authenticationToken
        self.i64()             # timestamp
        handle = self.u32()    # requestHandle
        self.u32()             # returnDiagnostics
        self.string()          # auditEntryId
        self.u32()             # timeoutHint
        self.extobj()          # additionalHeader
        return handle

    def application_description(self):
        self.string()
        self.string()
        self.localized_text()
        self.i32()
        self.string()
        self.string()
        n = self.i32()
        for _ in range(max(n, 0)):
            self.string()


def datetime_now_ticks():
    """100 ns ticks since 1601-01-01 UTC."""
    unix_us = time.time()
    seconds_1601_to_1970 = 11644473600
    return int((unix_us + seconds_1601_to_1970) * 10_000_000)


# --------------------------------------------------------------------------
# Simulated device
# --------------------------------------------------------------------------

class Device:
    def __init__(self, name):
        self.name = name
        self.setpoint = 21.5
        self.relay = False

    def value(self, ns, ident):
        """Returns (vtype, value) or None when the node does not exist."""
        if ns == 0 and ident == 2258:
            return VT_DATETIME, datetime_now_ticks()
        if ns != 1:
            return None
        uptime = time.time() - BOOT_TIME
        sine = 50.0 + 50.0 * math.sin(uptime / 60.0 * 2 * math.pi)
        temp = 20.0 + 6.0 * math.sin(uptime / 300.0 * 2 * math.pi)
        table = {
            "DeviceName": (VT_STRING, self.name),
            "Temperature": (VT_DOUBLE, round(temp, 3)),
            "Uptime": (VT_DOUBLE, round(uptime, 3)),
            "Sine": (VT_DOUBLE, round(sine, 3)),
            "Setpoint": (VT_DOUBLE, self.setpoint),
            "RelayOutput": (VT_BOOLEAN, self.relay),
        }
        if isinstance(ident, str):
            return table.get(ident)
        return None

    def write(self, ns, ident, value):
        if ns != 1 or not isinstance(ident, str):
            return False
        if ident == "Setpoint" and value is not None:
            self.setpoint = float(value)
            print("  write Setpoint = %.3f" % self.setpoint)
            return True
        if ident == "RelayOutput" and value is not None:
            self.relay = bool(value)
            print("  write RelayOutput = %s" % self.relay)
            return True
        return False


# --------------------------------------------------------------------------
# Session / connection handling
# --------------------------------------------------------------------------

class Session:
    def __init__(self, conn, addr, device, url, require_user):
        self.conn = conn
        self.addr = addr
        self.device = device
        self.url = url
        self.require_user = require_user
        self.channel_id = 0
        self.token_id = 1
        self.token = (0, 0)
        self.seq = 0
        self.req_id = 0
        self.handle = 0
        self.authenticated = False
        self.username = None
        self.req_handle = 0

    # --- framing ----------------------------------------------------------

    def send_frame(self, kind, body):
        self.conn.sendall(kind + struct.pack("<I", 8 + len(body)) + body)

    def read_frame(self):
        header = self._recv_exact(8)
        kind = header[:4]
        size = struct.unpack("<I", header[4:])[0]
        return kind, self._recv_exact(size - 8)

    def _recv_exact(self, n):
        data = b""
        while len(data) < n:
            chunk = self.conn.recv(n - len(data))
            if not chunk:
                raise ConnectionError("closed")
            data += chunk
        return data

    def next_ids(self):
        self.seq += 1
        self.req_id += 1
        return self.seq, self.req_id

    # --- services ---------------------------------------------------------

    def run(self):
        try:
            self.hello()
            while True:
                kind, payload = self.read_frame()
                if kind == b"OPNF":
                    self.open_channel(payload)
                elif kind == b"MSGF":
                    if not self.service(payload):
                        break
                elif kind == b"CLOF":
                    print("  close secure channel")
                    break
                else:
                    print("  unexpected frame %r" % kind)
                    break
        except (ConnectionError, DecodeError, socket.timeout) as exc:
            print("  session ended: %s" % exc)
        finally:
            self.conn.close()

    def hello(self):
        kind, payload = self.read_frame()
        if kind != b"HELF":
            raise DecodeError("expected HELF")
        r = R(payload)
        r.u32()
        rbs = r.u32()
        sbs = min(r.u32(), 65536)
        w = W()
        w.u32(0)         # protocolVersion
        w.u32(sbs)       # receiveBufferSize
        w.u32(sbs)       # sendBufferSize
        w.u32(0)         # maxMessageSize
        w.u32(0)         # maxChunkCount
        self.send_frame(b"ACKF", w.b)

    def open_channel(self, payload):
        r = R(payload)
        r.u32()                 # requested channel id
        r.string()              # policy
        r.bytestring()
        r.bytestring()
        r.u32()                 # sequence
        r.u32()                 # requestId
        node = r.nodeid()
        if node[1] != SVC_OPEN_CHANNEL:
            raise DecodeError("expected OpenSecureChannelRequest")
        self.req_handle = r.request_header()
        r.u32()                 # clientProtocolVersion
        r.i32()                 # requestType
        r.i32()                 # securityMode

        self.channel_id += 1
        self.token_id += 1
        seq, req_id = self.next_ids()
        w = W()
        w.u32(self.channel_id)
        w.string(POLICY_NONE)
        w.bytestring(None)
        w.bytestring(None)
        w.u32(seq)
        w.u32(req_id)
        w.nodeid(0, SVC_OPEN_CHANNEL + 1)
        self.write_response_header(w)
        w.u32(0)                # serverProtocolVersion
        w.u32(self.channel_id)  # securityToken.channelId
        w.u32(self.token_id)    # securityToken.tokenId
        w.i64(datetime_now_ticks())
        w.u32(600000)           # revisedLifetime
        w.bytestring(None)      # serverNonce
        self.send_frame(b"OPNF", w.b)

    def service(self, payload):
        r = R(payload)
        r.u32()                 # channelId
        r.u32()                 # tokenId
        r.u32()                 # sequenceNumber
        r.u32()                 # requestId
        node = r.nodeid()
        service = node[1]

        if service == SVC_GET_ENDPOINTS:
            self.req_handle = r.request_header()
            r.string()          # endpointUrl
            n = r.i32()
            for _ in range(max(n, 0)):
                r.string()
            n = r.i32()
            for _ in range(max(n, 0)):
                r.string()
            self.respond(service + 1, self.endpoints_body)
            return True

        if service == SVC_CREATE_SESSION:
            self.req_handle = r.request_header()
            r.application_description()
            r.string()          # serverUri
            r.string()          # endpointUrl
            r.string()          # sessionName
            r.bytestring()      # clientNonce
            r.bytestring()      # clientCertificate
            r.f64()             # requestedSessionTimeout
            r.u32()             # maxResponseMessageSize
            self.respond(service + 1, self.create_session_body)
            return True

        if service == SVC_ACTIVATE_SESSION:
            self.req_handle = r.request_header()
            r.string()          # clientSignature.algorithm
            r.bytestring()      # clientSignature.signature
            n = r.i32()
            for _ in range(max(n, 0)):
                r.bytestring()
                r.bytestring()
            n = r.i32()         # localeIds
            for _ in range(max(n, 0)):
                r.string()
            token_body = r.extobj()   # userIdentityToken
            r.string()          # userTokenSignature.algorithm
            r.bytestring()      # userTokenSignature.signature
            self.activate(token_body)
            return True

        if service == SVC_READ:
            self.req_handle = r.request_header()
            r.f64()             # maxAge
            r.i32()             # timestampsToReturn
            n = r.i32()
            values = []
            for _ in range(max(n, 0)):
                ns, ident = r.nodeid()
                r.u32()         # attributeId
                r.string()      # indexRange
                r.qualified_name()   # dataEncoding
                found = self.device.value(ns, ident)
                values.append(found)
            self.respond(service + 1, lambda w: self.read_body(w, values))
            return True

        if service == SVC_WRITE:
            self.req_handle = r.request_header()
            n = r.i32()
            codes = []
            for _ in range(max(n, 0)):
                ns, ident = r.nodeid()
                r.u32()         # attributeId
                r.string()      # indexRange
                value = r.datavalue()
                codes.append(0 if self.device.write(ns, ident, value) else 0x803A0000)
            self.respond(service + 1, lambda w: self.status_array_body(w, codes))
            return True

        if service == SVC_CLOSE_SESSION:
            self.req_handle = r.request_header()
            r.boolean()
            self.respond(service + 1, lambda w: w.u32(0))
            print("  close session")
            return False

        print("  unhandled service %d" % service)
        # Respond with BadServiceUnsupported to keep the client happy.
        seq, req_id = self.next_ids()
        w = W()
        w.u32(self.channel_id)
        w.u32(self.token_id)
        w.u32(seq)
        w.u32(req_id)
        w.nodeid(0, 0)
        self.write_response_header(w, 0x802B0000)   # BadServiceUnsupported
        self.send_frame(b"MSGF", w.b)
        return True

    # --- response builders --------------------------------------------------

    def write_response_header(self, w, status=0):
        """Real OPC UA ResponseHeader: timestamp, requestHandle,
        serviceResult, serviceDiagnostics, stringTable, additionalHeader."""
        w.i64(datetime_now_ticks())
        w.u32(self.req_handle)
        w.u32(status)
        w.u8(0x00)              # serviceDiagnostics (null)
        w.i32(-1)               # stringTable
        w.nodeid(0, 0)          # additionalHeader ExtensionObject typeId
        w.u8(0x00)              # ExtensionObject mask: no body

    def respond(self, resp_service, body_builder, status=0):
        seq, req_id = self.next_ids()
        w = W()
        w.u32(self.channel_id)
        w.u32(self.token_id)
        w.u32(seq)
        w.u32(req_id)
        w.nodeid(0, resp_service)
        self.write_response_header(w, status)
        body_builder(w)
        self.send_frame(b"MSGF", w.b)

    def endpoints_body(self, w):
        w.i32(1)
        w.endpoint_description(self.url, self.device.name)
        w.i32(-1)           # diagnosticInfos

    def create_session_body(self, w):
        w.nodeid(1, 1)          # sessionId
        w.nodeid(1, 100)        # authenticationToken
        w.i32(1)                # serverEndpoints
        w.endpoint_description(self.url, self.device.name)
        w.i32(-1)               # serverSoftwareCertificates
        w.string(None)          # serverSignature.algorithm
        w.bytestring(None)      # serverSignature.signature
        w.bytestring(None)      # serverNonce
        w.bytestring(None)      # serverCertificate
        w.u32(16777216)         # maxRequestMessageSize

    def activate(self, token_body):
        """Anonymous token (typeId 321) or Username token (typeId 322)."""
        if token_body is None:
            status = 0x80200000   # BadUserIdentityTokenInvalid
        else:
            tr = R(token_body)
            pid = tr.string()
            if pid == "anonymous":
                status = 0
            elif pid == "username":
                # typeId 322 body: userName string + password = extobj
                # (the password is passed inside an encrypted/NULL extobj)
                try:
                    user = tr.string()
                    tr.extobj()
                except DecodeError:
                    user = None
                if self.require_user and user != self.require_user:
                    status = 0x80200000
                else:
                    status = 0
            else:
                status = 0x80200000

        def body(w):
            w.i32(1)            # results
            w.u32(status)
            w.i32(-1)           # diagnosticInfos
            w.bytestring(None)  # serverNonce
            w.string(None)      # serverSignature.algorithm
            w.bytestring(None)  # serverSignature.signature

        self.respond(SVC_ACTIVATE_SESSION + 1, body, status=status)
        if status == 0:
            self.authenticated = True
            print("  session activated (%s)"
                  % ("anonymous" if not self.require_user else self.require_user))

    def read_body(self, w, values):
        w.i32(len(values))
        for found in values:
            if found is None:
                # value absent: status + both timestamps (mask 0x02|0x04|0x08)
                w.u8(0x02 | 0x04 | 0x08)
                w.u32(0x80380000)               # BadNodeIdUnknown
                w.i64(datetime_now_ticks())
                w.i64(datetime_now_ticks())
            else:
                vtype, value = found
                w.datavalue(vtype, value)
        w.i32(-1)           # diagnosticInfos

    def status_array_body(self, w, codes):
        w.i32(len(codes))
        for code in codes:
            w.u32(code)
        w.i32(-1)           # diagnosticInfos


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="Minimal OPC UA simulator (stdlib)")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=4840)
    ap.add_argument("--name", default="ESP32 OPC UA Simulator")
    ap.add_argument("--user", default=None,
                    help="require this username for session activation")
    args = ap.parse_args()

    global url
    url = "opc.tcp://127.0.0.1:%d" % args.port

    device = Device(args.name)
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.host, args.port))
    server.listen(4)
    print("Minimal OPC UA simulator on %s" % url)
    print("Tags: ns=1;s=DeviceName Temperature Uptime Sine Setpoint* RelayOutput* "
          "(* writable), ns=0;i=2258")
    print("Ctrl+C to stop.")

    try:
        while True:
            conn, addr = server.accept()
            print("connect from %s:%d" % addr[:2])
            session = Session(conn, addr, device, url, args.user)
            threading.Thread(target=session.run, daemon=True).start()
    except KeyboardInterrupt:
        print("bye")
        sys.exit(0)


if __name__ == "__main__":
    main()
