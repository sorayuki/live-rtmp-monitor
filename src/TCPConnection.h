#pragma once

#include <stdint.h>
#include <string>
#include <memory>
#include <optional>
#include <vector>

class TCPConnectionStatistics {
    int rtt_ms = 0;
    int64_t sent_bytes = 0;
    int64_t received_bytes = 0;
    int64_t retransmitted_bytes = 0;
};


class TCPConnection {
public:
    virtual ~TCPConnection() = default;

    virtual int getIpVersion() const = 0;
    virtual std::string getLocalAddress() const = 0;
    virtual uint16_t getLocalPort() const = 0;
    virtual std::string getRemoteAddress() const = 0;
    virtual uint16_t getRemotePort() const = 0;

    virtual bool enableStatistics() = 0;
    virtual std::optional<TCPConnectionStatistics> getStatistics() const = 0;
};

using TCPConnectionPtr = std::unique_ptr<TCPConnection>;

std::vector<TCPConnectionPtr> getTCPConnections();