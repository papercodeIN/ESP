#!/usr/bin/env python3
"""
Lightweight BACnet/IP traffic sniffer (standard library only).

Listens on UDP 47808 and prints every BACnet packet with its source address,
so you can identify who is sending (or flooding) what on the network. It ends
with a per-IP summary of packets by type.

    python tools/bacnet_sniffer.py               # run until Ctrl+C
    python tools/bacnet_sniffer.py --seconds 30  # auto-stop after 30 s

Close YABE and other BACnet tools first: only one program can own UDP 47808.
"""

from __future__ import annotations

import argparse
import socket
import sys
import time
from collections import defaultdict

PDU_TYPES = {
    0x00: "ConfirmedReq",
    0x10: "UnconfirmedReq",
    0x20: "SimpleAck",
    0x30: "ComplexAck",
    0x50: "Error",
    0x60: "Reject",
    0x70: "Abort",
}

CONFIRMED_SERVICES = {
    5: "SubscribeCOV",
    12: "ReadProperty",
    14: "ReadPropertyMultiple",
    15: "WriteProperty",
}

UNCONFIRMED_SERVICES = {0: "I-Am", 8: "Who-Is"}

BVLC_FUNCTIONS = {
    0x04: "ForwardedNPDU",
    0x09: "DistributeBroadcast",
    0x0A: "Orig-Unicast",
    0x0B: "Orig-Broadcast",
}


def strip_bvlc(packet: bytes):
    """Return (apdu, bvlc_function_name) or (None, None) for non-BACnet."""
    if len(packet) < 6 or packet[0] != 0x81:
        return None, None
    function = packet[1]
    length = int.from_bytes(packet[2:4], "big")
    if length < 4 or length > len(packet) or packet[4] != 0x01:
        return None, None
    return packet[6:length], BVLC_FUNCTIONS.get(function, f"0x{function:02X}")


def describe(apdu: bytes) -> str:
    if not apdu:
        return "(empty)"
    pdu = apdu[0] & 0xF0
    kind = PDU_TYPES.get(pdu, f"PDU 0x{apdu[0]:02X}")
    if pdu == 0x10 and len(apdu) >= 2:
        kind += ":" + UNCONFIRMED_SERVICES.get(apdu[1], f"svc{apdu[1]}")
    elif pdu == 0x00 and len(apdu) >= 4:
        kind += ":" + CONFIRMED_SERVICES.get(apdu[3], f"svc{apdu[3]}")
    elif pdu in (0x20, 0x30, 0x50) and len(apdu) >= 3:
        kind += f":svc{apdu[2]}"
    elif pdu == 0x60 and len(apdu) >= 3:
        kind += f"(reason {apdu[2]})"
    elif pdu == 0x70 and len(apdu) >= 3:
        kind += f"(reason {apdu[2]})"
    return kind


def main() -> int:
    parser = argparse.ArgumentParser(description="BACnet/IP sniffer")
    parser.add_argument("--port", type=int, default=47808)
    parser.add_argument(
        "--seconds", type=float, default=0,
        help="stop after N seconds (0 = run until Ctrl+C)")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("", args.port))
    except OSError as exc:
        print(f"[!] Could not bind UDP {args.port}: {exc}")
        print("    Close YABE and any other BACnet tool, then try again.")
        return 1
    sock.settimeout(1.0)

    print(f"[*] Listening on UDP {args.port} — stop with Ctrl+C")
    counts: dict = defaultdict(lambda: defaultdict(int))
    start = time.time()

    try:
        while True:
            if args.seconds and time.time() - start >= args.seconds:
                break
            try:
                packet, addr = sock.recvfrom(2048)
            except socket.timeout:
                continue
            apdu, bvlc = strip_bvlc(packet)
            desc = describe(apdu) if apdu else "(non-BACnet)"
            counts[addr[0]][f"{bvlc or '?'} {desc}"] += 1
            stamp = time.strftime("%H:%M:%S")
            print(f"{stamp}  {addr[0]:>15}:{addr[1]:<5}  {bvlc or '?':<16} {desc}")
    except KeyboardInterrupt:
        pass

    print("\n[*] Summary by source IP (who sent what):")
    ordered = sorted(counts.items(), key=lambda kv: -sum(kv[1].values()))
    if not ordered:
        print("    (no BACnet traffic seen)")
    for ip, services in ordered:
        total = sum(services.values())
        top = ", ".join(
            f"{name} x{count}"
            for name, count in sorted(services.items(), key=lambda kv: -kv[1])[:6])
        print(f"    {ip:<16} {total:>6} packets   {top}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
