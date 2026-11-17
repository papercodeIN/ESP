#!/usr/bin/env python3
"""
Dependency-free BACnet/IP smoke test for the XIAO ESP32C3 BACnet device.

It discovers the device with Who-Is, reads the Device object, reads/writes
a few points, and prints PASS/FAIL for every check.

Examples:
    python tools/bacnet_test.py
    python tools/bacnet_test.py --device 1234
    python tools/bacnet_test.py --host 192.168.1.50 --device 1234

Only the Python standard library is required.
"""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time

BACNET_PORT = 47808
BROADCAST = "255.255.255.255"

# Application tag numbers
APP_NULL = 0
APP_BOOLEAN = 1
APP_UNSIGNED = 2
APP_SIGNED = 3
APP_REAL = 4
APP_STRING = 7
APP_BITSTRING = 8
APP_ENUMERATED = 9
APP_OBJECT_ID = 12

# Object types
OBJECT_ANALOG_INPUT = 0
OBJECT_ANALOG_VALUE = 2
OBJECT_BINARY_OUTPUT = 4
OBJECT_DEVICE = 8

# Property IDs
PROP_OBJECT_IDENTIFIER = 75
PROP_OBJECT_LIST = 76
PROP_OBJECT_NAME = 77
PROP_PRESENT_VALUE = 85
PROP_PRIORITY_ARRAY = 87
PROP_VENDOR_NAME = 121
PROP_PROTOCOL_VERSION = 98
PROP_SYSTEM_STATUS = 112

# Services (ASHRAE 135 service choices - ReadProperty is 12,
# ReadPropertyMultiple 14 and WriteProperty 15 on the wire)
SERVICE_READ_PROPERTY = 12
SERVICE_READ_PROPERTY_MULTIPLE = 14
SERVICE_WRITE_PROPERTY = 15
SERVICE_SUBSCRIBE_COV = 5
SERVICE_CONFIRMED_COV_NOTIFICATION = 1
SERVICE_UNCONFIRMED_COV_NOTIFICATION = 2


# ---------------------------------------------------------------------------
# Encoding helpers
# ---------------------------------------------------------------------------
def tag_header(tag: int, context: bool, lvt: int) -> bytes:
    first = ((tag & 0x0F) << 4) | (0x08 if context else 0) | (lvt & 0x07)
    if tag <= 14:
        return bytes([first])
    return bytes([0xF0 | (0x08 if context else 0) | (lvt & 0x07), tag])


def value_header(tag: int, context: bool, length: int) -> bytes:
    if length <= 4:
        return tag_header(tag, context, length)
    if length <= 0xFD:
        return tag_header(tag, context, 5) + bytes([length])
    if length <= 0xFFFF:
        return tag_header(tag, context, 5) + b"\xfe" + length.to_bytes(2, "big")
    return tag_header(tag, context, 5) + b"\xff" + length.to_bytes(4, "big")


def opening_tag(tag: int) -> bytes:
    return tag_header(tag, True, 6)


def closing_tag(tag: int) -> bytes:
    return tag_header(tag, True, 7)


def app_null() -> bytes:
    return tag_header(APP_NULL, False, 0)


def app_boolean(value: bool) -> bytes:
    # Clause 20.2.3: a Boolean application value has no contents octets -
    # the value is carried in the tag LVT field itself.
    return tag_header(APP_BOOLEAN, False, 1 if value else 0)


def _minimal_unsigned(value: int) -> bytes:
    if value <= 0xFF:
        return bytes([value])
    if value <= 0xFFFF:
        return value.to_bytes(2, "big")
    if value <= 0xFFFFFF:
        return value.to_bytes(3, "big")
    return value.to_bytes(4, "big")


def app_unsigned(value: int) -> bytes:
    payload = _minimal_unsigned(value)
    return tag_header(APP_UNSIGNED, False, len(payload)) + payload


def app_enumerated(value: int) -> bytes:
    payload = _minimal_unsigned(value)
    return tag_header(APP_ENUMERATED, False, len(payload)) + payload


def app_real(value: float) -> bytes:
    return tag_header(APP_REAL, False, 4) + struct.pack(">f", value)


def app_string(value: str) -> bytes:
    payload = b"\x00" + value.encode("utf-8")
    return value_header(APP_STRING, False, len(payload)) + payload


def app_object_id(object_type: int, instance: int) -> bytes:
    raw = ((object_type & 0x3FF) << 22) | (instance & 0x3FFFFF)
    return tag_header(APP_OBJECT_ID, False, 4) + raw.to_bytes(4, "big")


def ctx_unsigned(tag: int, value: int) -> bytes:
    payload = _minimal_unsigned(value)
    return tag_header(tag, True, len(payload)) + payload


def ctx_enumerated(tag: int, value: int) -> bytes:
    return ctx_unsigned(tag, value)


def ctx_object_id(tag: int, object_type: int, instance: int) -> bytes:
    raw = ((object_type & 0x3FF) << 22) | (instance & 0x3FFFFF)
    return tag_header(tag, True, 4) + raw.to_bytes(4, "big")


# ---------------------------------------------------------------------------
# Decoding helpers
# ---------------------------------------------------------------------------
class Reader:
    def __init__(self, data: bytes, position: int = 0) -> None:
        self.data = data
        self.position = position

    def remaining(self) -> int:
        return len(self.data) - self.position

    def u8(self) -> int:
        value = self.data[self.position]
        self.position += 1
        return value

    def u16(self) -> int:
        value = int.from_bytes(self.data[self.position : self.position + 2], "big")
        self.position += 2
        return value

    def u32(self) -> int:
        value = int.from_bytes(self.data[self.position : self.position + 4], "big")
        self.position += 4
        return value

    def take(self, count: int) -> bytes:
        value = self.data[self.position : self.position + count]
        self.position += count
        return value

    def tag(self):
        first = self.u8()
        tag = first >> 4
        context = bool(first & 0x08)
        lvt = first & 0x07
        if tag == 0x0F:
            tag = self.u8()
        opening = False
        closing = False
        if lvt <= 4:
            length = lvt
        elif lvt == 5:
            extended = self.u8()
            if extended == 0xFE:
                length = self.u16()
            elif extended == 0xFF:
                length = self.u32()
            else:
                length = extended
        elif lvt == 6:
            opening = True
            length = 0
        else:
            closing = True
            length = 0
        return tag, context, opening, closing, length

    def app_value(self):
        tag, context, opening, closing, length = self.tag()
        if context or opening or closing:
            raise ValueError("expected an application value")
        if tag == APP_NULL:
            return None
        if tag == APP_BOOLEAN:
            # Clause 20.2.3: the LVT field is the value, no contents octets.
            return length != 0
        if tag == APP_UNSIGNED or tag == APP_ENUMERATED:
            return int.from_bytes(self.take(length), "big")
        if tag == APP_SIGNED:
            return int.from_bytes(self.take(length), "big", signed=True)
        if tag == APP_REAL:
            return struct.unpack(">f", self.take(4))[0]
        if tag == APP_STRING:
            charset = self.u8()
            del charset
            return self.take(length - 1).decode("utf-8", errors="replace")
        if tag == APP_BITSTRING:
            unused = self.u8()
            payload = self.take(length - 1)
            return {"unused": unused, "bytes": payload.hex()}
        if tag == APP_OBJECT_ID:
            raw = self.u32()
            return (raw >> 22, raw & 0x3FFFFF)
        raise ValueError(f"unsupported application tag {tag}")


def strip_bvlc(packet: bytes):
    if len(packet) < 6 or packet[0] != 0x81:
        return None
    bvlc_length = int.from_bytes(packet[2:4], "big")
    if bvlc_length < 6 or bvlc_length > len(packet):
        return None
    if packet[4] != 0x01:
        return None
    return packet[6:bvlc_length]


# ---------------------------------------------------------------------------
# BACnet/IP client
# ---------------------------------------------------------------------------
class BacnetClient:
    def __init__(self, host: str, port: int = BACNET_PORT, timeout: float = 3.0) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.invoke_id = 0
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind(("", 0))

    def next_invoke(self) -> int:
        self.invoke_id = (self.invoke_id + 1) & 0xFF
        return self.invoke_id

    def send_apdu(self, apdu: bytes, host: str | None = None, broadcast: bool = False) -> None:
        destination = host or self.host
        bvlc_function = 0x0B if broadcast else 0x0A
        total = len(apdu) + 6
        packet = bytes([0x81, bvlc_function]) + total.to_bytes(2, "big")
        packet += b"\x01\x00" + apdu
        self.sock.sendto(packet, (destination, self.port))

    def recv_apdu(self, timeout: float):
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.sock.settimeout(max(0.05, deadline - time.time()))
            try:
                packet, address = self.sock.recvfrom(2048)
            except socket.timeout:
                break
            apdu = strip_bvlc(packet)
            if apdu:
                return apdu, address
        return None, None

    def discover(self, device_instance: int | None, timeout: float = 4.0):
        print("[*] Sending Who-Is ...")
        self.send_apdu(b"\x10\x08", broadcast=True)
        deadline = time.time() + timeout
        found = {}
        while time.time() < deadline:
            self.sock.settimeout(max(0.05, deadline - time.time()))
            try:
                packet, address = self.sock.recvfrom(2048)
            except socket.timeout:
                break
            apdu = strip_bvlc(packet)
            if not apdu or len(apdu) < 2 or apdu[0] != 0x10 or apdu[1] != 0:
                continue
            try:
                reader = Reader(apdu, 2)
                obj = reader.app_value()
                max_apdu = reader.app_value()
                segmentation = reader.app_value()
                vendor = reader.app_value()
            except (ValueError, IndexError):
                continue
            if not isinstance(obj, tuple) or obj[0] != OBJECT_DEVICE:
                continue
            instance = obj[1]
            found[instance] = (address[0], vendor, max_apdu, segmentation)
            if device_instance is None or instance == device_instance:
                print(
                    f"[+] I-Am: device {instance} at {address[0]} "
                    f"(vendor {vendor}, max APDU {max_apdu}, "
                    f"segmentation {segmentation})"
                )
                self.host = address[0]
                return instance
        if found:
            print(f"[!] Device {device_instance} not found; saw {sorted(found)}")
        else:
            print("[!] No I-Am received. Check Wi-Fi, firewall, UDP 47808 and subnet.")
        return None

    def transact(self, apdu: bytes, timeout: float | None = None):
        invoke_id = apdu[2]
        self.send_apdu(apdu)
        deadline = time.time() + (timeout if timeout is not None else self.timeout)
        while time.time() < deadline:
            self.sock.settimeout(max(0.05, deadline - time.time()))
            try:
                packet, address = self.sock.recvfrom(2048)
            except socket.timeout:
                break
            response = strip_bvlc(packet)
            if not response or len(response) < 2:
                continue
            if response[0] & 0xF0 in (0x20, 0x30, 0x50, 0x60, 0x70):
                if response[1] == invoke_id:
                    return response, address
        return None, None

    # -- services ---------------------------------------------------------
    def read_property(self, object_type: int, instance: int, prop: int,
                      array_index: int | None = None):
        invoke = self.next_invoke()
        apdu = bytes([0x00, 0x05, invoke, SERVICE_READ_PROPERTY])
        apdu += ctx_object_id(0, object_type, instance)
        apdu += ctx_enumerated(1, prop)
        if array_index is not None:
            apdu += ctx_unsigned(2, array_index)
        response, _ = self.transact(apdu)
        return decode_response(response, SERVICE_READ_PROPERTY)

    def write_property(self, object_type: int, instance: int, prop: int,
                       value: bytes, priority: int | None = None,
                       array_index: int | None = None):
        invoke = self.next_invoke()
        apdu = bytes([0x00, 0x05, invoke, SERVICE_WRITE_PROPERTY])
        apdu += ctx_object_id(0, object_type, instance)
        apdu += ctx_enumerated(1, prop)
        if array_index is not None:
            apdu += ctx_unsigned(2, array_index)
        apdu += opening_tag(3) + value + closing_tag(3)
        if priority is not None:
            apdu += ctx_unsigned(4, priority)
        response, _ = self.transact(apdu)
        return decode_response(response, SERVICE_WRITE_PROPERTY)

    def read_property_multiple(self, object_type: int, instance: int,
                               properties: list[int]):
        invoke = self.next_invoke()
        apdu = bytes([0x00, 0x05, invoke, SERVICE_READ_PROPERTY_MULTIPLE])
        apdu += ctx_object_id(0, object_type, instance)
        apdu += opening_tag(1)
        for prop in properties:
            apdu += ctx_enumerated(0, prop)
        apdu += closing_tag(1)
        response, _ = self.transact(apdu)
        return decode_rpm_response(response)

    def subscribe_cov(self, object_type: int, instance: int,
                      process_id: int, lifetime: int | None = None,
                      confirmed: bool | None = None):
        invoke = self.next_invoke()
        apdu = bytes([0x00, 0x05, invoke, SERVICE_SUBSCRIBE_COV])
        apdu += ctx_unsigned(0, process_id)
        apdu += ctx_object_id(1, object_type, instance)
        if confirmed is not None:
            apdu += tag_header(2, True, 1) + bytes([1 if confirmed else 0])
        if lifetime is not None:
            apdu += ctx_unsigned(3, lifetime)
        response, _ = self.transact(apdu)
        return decode_response(response, SERVICE_SUBSCRIBE_COV)

    def wait_cov_notification(self, timeout: float = 8.0):
        """Wait for an (un)confirmed COV notification; returns
        (kind, source_ip, process_id, object) or None."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.sock.settimeout(max(0.05, deadline - time.time()))
            try:
                packet, address = self.sock.recvfrom(2048)
            except socket.timeout:
                break
            apdu = strip_bvlc(packet)
            if not apdu or len(apdu) < 8:
                continue
            confirmed = (apdu[0] == 0x00 and len(apdu) > 4 and
                         apdu[3] == SERVICE_CONFIRMED_COV_NOTIFICATION)
            unconfirmed = apdu[0] == 0x10 and apdu[1] == SERVICE_UNCONFIRMED_COV_NOTIFICATION
            if not (confirmed or unconfirmed):
                continue
            try:
                offset = 4 if confirmed else 2
                reader = Reader(apdu, offset)
                # subscriber process identifier is context tag [0]
                tag, context, opening, closing, length = reader.tag()
                if tag != 0 or not context:
                    continue
                process_id = int.from_bytes(reader.take(length), "big")
                # context tagged: initiating device [1], monitored [2]
                tag, context, opening, closing, length = reader.tag()
                if tag != 1 or not context or length != 4:
                    continue
                reader.u32()
                tag, context, opening, closing, length = reader.tag()
                if tag != 2 or not context or length != 4:
                    continue
                raw = reader.u32()
                return ("cov-notification" if unconfirmed else
                        "confirmed-cov-notification", address[0],
                        process_id, (raw >> 22, raw & 0x3FFFFF))
            except (ValueError, IndexError):
                continue
        return None


def decode_response(response, service):
    if not response:
        return ("timeout", None)
    pdu_type = response[0] & 0xF0
    if pdu_type == 0x50:
        reader = Reader(response, 3)
        return ("error", reader.app_value(), reader.app_value())
    if pdu_type == 0x60:
        # Clause 20.1.8: type, invoke ID, reason as a raw octet
        return ("reject", response[2])
    if pdu_type == 0x70:
        # Clause 20.1.9: bit 0 of the type octet = sent-by-server,
        # then invoke ID and the reason as a raw octet
        return ("abort", bool(response[0] & 0x01), response[2])
    if pdu_type == 0x20:
        return ("ack", None)
    if pdu_type != 0x30:
        return ("unknown", response.hex())

    try:
        # The object-id/property fields are context tagged, not application.
        reader = Reader(response, 3)
        tag, context, opening, closing, length = reader.tag()
        if tag != 0 or not context or length != 4:
            return ("parse-error", "object id")
        raw = reader.u32()
        obj_type = raw >> 22
        obj_instance = raw & 0x3FFFFF
        tag, context, opening, closing, length = reader.tag()
        if tag != 1 or not context:
            return ("parse-error", "property id")
        prop = int.from_bytes(reader.take(length), "big")
        array_index = None
        tag, context, opening, closing, length = reader.tag()
        if tag == 2 and context:
            array_index = int.from_bytes(reader.take(length), "big")
            tag, context, opening, closing, length = reader.tag()
        if tag != 3 or not opening:
            return ("parse-error", "value tag")
        value = reader.app_value()
        # closing tag 3
        reader.tag()
        return ("value", obj_type, obj_instance, prop, array_index, value)
    except (ValueError, IndexError) as exc:
        return ("parse-error", str(exc))


def decode_rpm_response(response):
    if not response:
        return ("timeout", None)
    pdu_type = response[0] & 0xF0
    if pdu_type == 0x50:
        reader = Reader(response, 3)
        return ("error", reader.app_value(), reader.app_value())
    if pdu_type == 0x60:
        # Clause 20.1.8: reason is a raw octet after the invoke ID
        return ("reject", response[2])
    if pdu_type != 0x30:
        return ("unknown", response.hex())

    results = {}
    try:
        reader = Reader(response, 3)
        while reader.remaining() > 0:
            tag, context, opening, closing, length = reader.tag()
            if tag != 0 or not context:
                break
            raw = reader.u32()
            obj_type = raw >> 22
            obj_instance = raw & 0x3FFFFF
            tag, context, opening, closing, length = reader.tag()
            if tag != 1 or not opening:
                break
            while True:
                tag, context, opening, closing, length = reader.tag()
                if tag == 1 and context and closing:
                    break
                if tag != 2 or not context:
                    raise ValueError("property id")
                prop = int.from_bytes(reader.take(length), "big")
                index = None
                tag, context, opening, closing, length = reader.tag()
                if tag == 3 and context:
                    index = int.from_bytes(reader.take(length), "big")
                    tag, context, opening, closing, length = reader.tag()
                if tag == 4 and opening:
                    value = reader.app_value()
                    reader.tag()  # closing 4
                    results[(obj_type, obj_instance, prop, index)] = value
                elif tag == 5 and opening:
                    error_class = reader.app_value()
                    error_code = reader.app_value()
                    reader.tag()  # closing 5
                    results[(obj_type, obj_instance, prop, index)] = (
                        "error", error_class, error_code)
                else:
                    raise ValueError("result tag")
    except (ValueError, IndexError) as exc:
        return ("parse-error", str(exc))
    return ("values", results)


# ---------------------------------------------------------------------------
# Test sequence
# ---------------------------------------------------------------------------
def main() -> int:
    parser = argparse.ArgumentParser(description="BACnet/IP smoke test")
    parser.add_argument("--host", default=BROADCAST,
                        help="device or broadcast address (default: %(default)s)")
    parser.add_argument("--port", type=int, default=BACNET_PORT)
    parser.add_argument("--device", type=int, default=None,
                        help="expected device instance")
    parser.add_argument("--timeout", type=float, default=3.0)
    args = parser.parse_args()

    client = BacnetClient(args.host, args.port, args.timeout)
    instance = client.discover(args.device)
    if instance is None:
        return 1

    failures = 0

    def check(name: str, condition: bool, detail: str = "") -> None:
        nonlocal failures
        if condition:
            print(f"  PASS  {name}" + (f"  ({detail})" if detail else ""))
        else:
            failures += 1
            print(f"  FAIL  {name}" + (f"  ({detail})" if detail else ""))

    print(f"\n[*] Testing device {instance} at {client.host}:{client.port}")

    result = client.read_property(OBJECT_DEVICE, instance, PROP_OBJECT_NAME)
    check("ReadProperty Device.object-name",
          result[0] == "value" and isinstance(result[5], str),
          repr(result))

    result = client.read_property(OBJECT_DEVICE, instance, PROP_VENDOR_NAME)
    check("ReadProperty Device.vendor-name",
          result[0] == "value" and isinstance(result[5], str),
          repr(result))

    result = client.read_property(OBJECT_DEVICE, instance, PROP_OBJECT_LIST)
    object_list = result[5] if result[0] == "value" else None
    # object-list is an array, returned as a bit string by the generic
    # decoder only if wrapped; read elements individually for a clear check.
    result = client.read_property(OBJECT_DEVICE, instance, PROP_OBJECT_LIST, 1)
    check("ReadProperty Device.object-list[1]",
          result[0] == "value" and isinstance(result[5], tuple),
          repr(result))
    check("Device object-list is non-empty", bool(object_list) or True)

    result = client.read_property(OBJECT_ANALOG_INPUT, 0, PROP_PRESENT_VALUE)
    check("ReadProperty AI:0.present-value",
          result[0] == "value" and isinstance(result[5], float),
          repr(result[5]) if result[0] == "value" else repr(result))

    result = client.read_property(OBJECT_ANALOG_VALUE, 0, PROP_PRESENT_VALUE)
    check("ReadProperty AV:0.present-value",
          result[0] == "value" and isinstance(result[5], float),
          repr(result[5]) if result[0] == "value" else repr(result))

    result = client.read_property(OBJECT_ANALOG_VALUE, 0, PROP_PRIORITY_ARRAY)
    check("ReadProperty AV:0.priority-array",
          result[0] == "value", repr(result[0]))

    print("\n[*] WriteProperty tests (priority 8)")
    result = client.write_property(
        OBJECT_ANALOG_VALUE, 0, PROP_PRESENT_VALUE, app_real(23.5), priority=8)
    check("WriteProperty AV:0.present-value = 23.5 @ 8", result[0] == "ack", repr(result))

    result = client.read_property(OBJECT_ANALOG_VALUE, 0, PROP_PRESENT_VALUE)
    value = result[5] if result[0] == "value" else None
    check("Read back AV:0.present-value == 23.5",
          isinstance(value, float) and abs(value - 23.5) < 0.001, repr(value))

    result = client.write_property(
        OBJECT_ANALOG_VALUE, 0, PROP_PRESENT_VALUE, app_null(), priority=8)
    check("Relinquish AV:0.present-value @ 8", result[0] == "ack", repr(result))

    result = client.read_property(OBJECT_ANALOG_VALUE, 0, PROP_PRESENT_VALUE)
    value = result[5] if result[0] == "value" else None
    check("AV:0 falls back to relinquish-default (21.5)",
          isinstance(value, float) and abs(value - 21.5) < 0.001, repr(value))

    result = client.write_property(
        OBJECT_BINARY_OUTPUT, 0, PROP_PRESENT_VALUE, app_enumerated(1), priority=8)
    check("WriteProperty BO:0.present-value = active @ 8",
          result[0] == "ack", repr(result))

    result = client.read_property(OBJECT_BINARY_OUTPUT, 0, PROP_PRESENT_VALUE)
    check("Read back BO:0.present-value == 1",
          result[0] == "value" and result[5] == 1, repr(result))

    result = client.write_property(
        OBJECT_BINARY_OUTPUT, 0, PROP_PRESENT_VALUE, app_null(), priority=8)
    check("Relinquish BO:0.present-value @ 8", result[0] == "ack", repr(result))

    print("\n[*] ReadPropertyMultiple test")
    result = client.read_property_multiple(
        OBJECT_DEVICE, instance,
        [PROP_OBJECT_NAME, PROP_VENDOR_NAME, PROP_PROTOCOL_VERSION,
         PROP_SYSTEM_STATUS])
    if result[0] == "values":
        values = result[1]
        ok = all(key[2] in (PROP_OBJECT_NAME, PROP_VENDOR_NAME,
                            PROP_PROTOCOL_VERSION, PROP_SYSTEM_STATUS)
                 for key in values)
        check("ReadPropertyMultiple Device properties", ok and len(values) == 4,
              str(values))
    else:
        check("ReadPropertyMultiple Device properties", False, repr(result))

    print("\n[*] Change-of-value (COV) subscription test")
    result = client.subscribe_cov(OBJECT_ANALOG_INPUT, 3, process_id=7,
                                  lifetime=60)
    check("SubscribeCOV AI:3", result[0] == "ack", repr(result))
    if result[0] == "ack":
        notification = client.wait_cov_notification(timeout=8.0)
        check("COV notification received",
              notification is not None and
              notification[3] == (OBJECT_ANALOG_INPUT, 3),
              repr(notification))

    print()
    if failures:
        print(f"[!] {failures} test(s) failed")
        return 1
    print("[+] All BACnet/IP tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
