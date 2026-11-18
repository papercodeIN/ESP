/**
 * @file bacnet_server.h
 * @brief Minimal BACnet/IP server for the XIAO ESP32C3.
 *
 * Supported services:
 *   - Who-Is / I-Am (device discovery)
 *   - ReadProperty
 *   - ReadPropertyMultiple
 *   - WriteProperty (with 16-level priority arrays)
 */
#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

#include "bacnet_encoding.h"
#include "bacnet_types.h"

namespace bacnet {

/* One active Change-of-Value subscription (SubscribeCOV, service 5). */
struct CovSubscription {
    bool active = false;
    uint32_t processId = 0;
    uint16_t objectType = 0;
    uint32_t objectInstance = 0;
    IPAddress address;
    uint16_t port = 0;
    bool confirmed = false;         /* subscriber wants confirmed notifications */
    uint32_t expiresAtMs = 0;       /* 0 = subscription without a lifetime */
    float lastRealValue = 0.0f;
    uint8_t lastBinaryValue = 0;
    bool hasLastValue = false;
    uint32_t lastNotifyMs = 0;
};

class Server {
  public:
    bool begin(uint16_t port = 47808);
    void run();

    /* Send an I-Am to the broadcast address (for manual discovery). */
    void sendIAm();

    uint16_t port() const { return port_; }
    void stop();
    uint32_t rxPackets() const { return rxPackets_; }
    uint32_t droppedPackets() const { return droppedPackets_; }
    uint32_t txErrors() const { return txErrors_; }
    size_t activeCovSubscriptions() const;

  private:
    void handleDatagram(const uint8_t *data,
                        size_t length,
                        IPAddress remoteIp,
                        uint16_t remotePort);
    void handleNpdu(const uint8_t *data,
                    size_t length,
                    IPAddress remoteIp,
                    uint16_t remotePort);
    void handleApdu(const uint8_t *data,
                    size_t length,
                    IPAddress remoteIp,
                    uint16_t remotePort);
    void handleWhoIs(const uint8_t *data,
                     size_t length,
                     IPAddress remoteIp,
                     uint16_t remotePort);
    void handleSubscribeCov(const uint8_t *data,
                            size_t length,
                            uint8_t invokeId,
                            IPAddress remoteIp,
                            uint16_t remotePort);
    void handleReadProperty(const uint8_t *data,
                            size_t length,
                            uint8_t invokeId,
                            IPAddress remoteIp,
                            uint16_t remotePort);
    void handleWriteProperty(const uint8_t *data,
                             size_t length,
                             uint8_t invokeId,
                             IPAddress remoteIp,
                             uint16_t remotePort);
    void handleReadPropertyMultiple(const uint8_t *data,
                                    size_t length,
                                    uint8_t invokeId,
                                    IPAddress remoteIp,
                                    uint16_t remotePort);

    /* Expire stale subscriptions and push notifications for points whose
     * value changed more than their COV increment. */
    void serviceCovSubscriptions();
    void sendCovNotification(CovSubscription &subscription);
    CovSubscription *findCovSubscription(uint32_t processId,
                                         uint16_t objectType,
                                         uint32_t objectInstance);
    CovSubscription *reserveCovSubscription();

    void sendIAmFrame(IPAddress destination, uint16_t destinationPort,
                      bool broadcast);
    void sendApdu(IPAddress destination, uint16_t destinationPort,
                  size_t apduLength, bool broadcast);
    void sendSimpleAck(uint8_t invokeId, uint8_t service,
                       IPAddress remoteIp, uint16_t remotePort);
    void sendError(uint8_t invokeId, uint8_t service,
                   uint16_t errorClass, uint32_t errorCode,
                   IPAddress remoteIp, uint16_t remotePort);
    void sendReject(uint8_t invokeId, uint8_t reason,
                    IPAddress remoteIp, uint16_t remotePort);
    void sendAbort(uint8_t invokeId, bool server, uint8_t reason,
                   IPAddress remoteIp, uint16_t remotePort);
    void sendReadError(ReadStatus status, uint8_t invokeId, uint8_t service,
                       IPAddress remoteIp, uint16_t remotePort);

    WiFiUDP udp_;
    uint16_t port_ = 47808;
    uint8_t rx_[1600] = {0};
    uint8_t tx_[1600] = {0};
    uint8_t apdu_[kMaxApduBuffer] = {0};
    uint8_t valueScratch_[600] = {0};

    static constexpr size_t kMaxCovSubscriptions = 8;
    CovSubscription cov_[kMaxCovSubscriptions];
    uint8_t invokeId_ = 0;

    uint32_t startupMs_ = 0;
    uint32_t lastStartupIAmMs_ = 0;
    uint32_t rxPackets_ = 0;
    uint32_t droppedPackets_ = 0;
    uint32_t txErrors_ = 0;
};

} // namespace bacnet
