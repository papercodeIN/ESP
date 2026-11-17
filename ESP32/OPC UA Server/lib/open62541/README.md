# open62541 (amalgamated, patched for ESP32)

The [open62541](https://www.open62541.org/) reference implementation of OPC UA
(IEC 62541), version **v1.4.22**, in its official single-file amalgamation
distribution (`open62541.c` + `open62541.h`), licensed under
[MPL-2.0](LICENSE).

The released amalgamation targets desktop systems, so two sets of changes
were applied for ESP32 / Arduino / lwIP. Everything else is byte-identical to
the official release.

## Config changes (`include/open62541.h`, baked-in build options)

| Option | Release value | ESP32 value | Reason |
| --- | --- | --- | --- |
| `UA_LOGLEVEL` | 100 (fatal only) | 400 (info and worse) | see what the stack is doing on serial |
| `UA_ENABLE_ENCRYPTION` / `UA_ENABLE_ENCRYPTION_MBEDTLS` | on | on (kept) | must stay on: unguarded PubSub security code requires it; mbedTLS is bundled with the ESP32 core. Security policy `None` is still what the firmware configures |
| `UA_ENABLE_PUBSUB` (+ `_ENCRYPTION`) | on | on (kept) | the amalgamation contains PubSub source files without internal guards, so these options cannot be turned off in the released amalgamation; the firmware never instantiates PubSub, so the cost is flash only |
| `UA_ENABLE_PUBSUB_ENCRYPTION` / `UA_ENABLE_PUBSUB_INFORMATIONMODEL` | on | off | not used; encryption would pull mbedTLS, the information model needs the full namespace 0 |
| `UA_ENABLE_DISCOVERY` / `UA_ENABLE_DISCOVERY_SEMAPHORE` | on | off | LDS registration is not used; the semaphore code needs POSIX `sem_open`, which ESP32 lacks |
| `UA_ENABLE_JSON_ENCODING` | on | off | binary OPC UA only; saves flash |
| `UA_MULTITHREADING` | 100 | 0 | the firmware drives `UA_Server_run_iterate()` itself; no internal worker threads |
| `UA_THREADSAFE` | on | off | the OPC UA stack runs in a single Arduino loop; locks are not needed |
| `UA_GENERATED_NAMESPACE_ZERO` (+ `_FULL`) | on (full) | off | the full standard namespace 0 (~2700 nodes) does not fit in ESP32 RAM; open62541's built-in minimal namespace 0 is used instead |
| `UA_HAS_GETIFADDR` | 1 | 0 | lwIP has no `getifaddrs()` (only used by disabled PubSub) |
| `#include <poll.h>` | poll.h | sys/poll.h | ESP32's lwIP exposes `poll()` through `sys/poll.h` only |
| `#include <fcntl.h>` / `#include <ifaddrs.h>` | included | removed | lwIP provides neither header; with this configuration neither API is referenced |
| `gai_strerror()` | used in socket error logs | `strerror()` | lwIP has no `gai_strerror`; the affected call sites log errors only |
| `pipe()` (InterruptManager self-pipe) | `pipe()` | bound UDP socket | ESP-IDF has no `pipe()`; no signals are registered, so a quiet, pollable socket replaces the self-pipe |
| `getnameinfo()` / `NI_*` flags | BSD netdb | local `inet_ntop()` helper + flag defines | lwIP has no `getnameinfo`; call sites only render numeric addresses |
| `getifaddrs()` (PubSub UDP multicast interface selection) | BSD ifaddrs | stub returning `BadNotSupported` on ESP32 | lwIP has no `ifaddrs.h`; the firmware never selects multicast interfaces |

## Source change (`src/open62541.c`)

`UA_EventLoopPOSIX_setNonBlocking()` used `fcntl(fd, F_SETFL, O_NONBLOCK)`,
which lwIP does not implement. Replaced with `ioctl(fd, FIONBIO, &on)`,
which `lwip_ioctl` supports.

## Regenerating / upgrading

Download the amalgamation assets of the wanted release from
<https://github.com/open62541/open62541/releases> and re-apply the changes
above. The changes are deliberately small and mechanical so they survive new
releases.
