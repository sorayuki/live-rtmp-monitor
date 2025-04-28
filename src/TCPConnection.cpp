#include "TCPConnection.h"

#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdexcept>

class TCPConnectionBase: public TCPConnection {
protected:
    int ipVersion_;
    std::string localAddress_;
    uint16_t localPort_;
    std::string remoteAddress_;
    uint16_t remotePort_;

public:
    virtual ~TCPConnectionBase() = default;

    int getIpVersion() const {
        return ipVersion_;
    }

    std::string getLocalAddress() const {
        return localAddress_;
    }

    uint16_t getLocalPort() const {
        return localPort_;
    }

    std::string getRemoteAddress() const {
        return remoteAddress_;
    }

    uint16_t getRemotePort() const {
        return remotePort_;
    }

    virtual bool enableStatistics() = 0;
    virtual std::optional<TCPConnectionStatistics> getStatistics() const = 0;
};


class TCP4Connection : public TCPConnectionBase {
    MIB_TCPROW tcpRow_;
    bool statisticsEnabled_ = false;

public:
    TCP4Connection(const MIB_TCPROW& tcpRow)
        : tcpRow_(tcpRow) 
    {
        ipVersion_ = 4;

        char addressBuffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &tcpRow_.dwLocalAddr, addressBuffer, sizeof(addressBuffer));
        localAddress_ = std::string(addressBuffer);
        localPort_ = ntohs(static_cast<uint16_t>(tcpRow_.dwLocalPort & 0xFFFF));

        inet_ntop(AF_INET, &tcpRow_.dwRemoteAddr, addressBuffer, sizeof(addressBuffer));
        remoteAddress_ = std::string(addressBuffer);
        remotePort_ = ntohs(static_cast<uint16_t>(tcpRow_.dwRemotePort & 0xFFFF));
    }

    ~TCP4Connection() override {
        if (statisticsEnabled_) {
            // Cleanup if needed
        }
    }

    bool enableStatistics() override {
        // Implementation here
    }

    std::optional<TCPConnectionStatistics> getStatistics() const override {
        // Implementation here
    }
};


class TCP6Connection : public TCPConnectionBase {
    MIB_TCP6ROW tcpRow_;
    bool statisticsEnabled_ = false;

public:
    TCP6Connection(const MIB_TCP6ROW& tcpRow)
        : tcpRow_(tcpRow)
    {
        ipVersion_ = 6;

        char addressBuffer[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &tcpRow_.LocalAddr, addressBuffer, sizeof(addressBuffer));
        localAddress_ = std::string(addressBuffer);
        localPort_ = ntohs(static_cast<uint16_t>(tcpRow_.dwLocalPort & 0xFFFF));

        inet_ntop(AF_INET6, &tcpRow_.RemoteAddr, addressBuffer, sizeof(addressBuffer));
        remoteAddress_ = std::string(addressBuffer);
        remotePort_ = ntohs(static_cast<uint16_t>(tcpRow_.dwRemotePort & 0xFFFF));
    }

    ~TCP6Connection() override {
        if (statisticsEnabled_) {
            // Cleanup if needed
        }
    }

    bool enableStatistics() override {
        // Implementation here
    }

    std::optional<TCPConnectionStatistics> getStatistics() const override {
        // Implementation here
    }
};


std::vector<TCPConnectionPtr> getTCPConnections() {
    std::vector<TCPConnectionPtr> connections;

    // Retrieve TCPv4 connections
    std::vector<uint8_t> tcpTableBuffer;
    DWORD size = 0;
    while (GetTcpTable(nullptr, &size, TRUE) == ERROR_INSUFFICIENT_BUFFER) {
        tcpTableBuffer.resize(size);
        PMIB_TCPTABLE tcpTable = reinterpret_cast<PMIB_TCPTABLE>(tcpTableBuffer.data());
        auto ret = GetTcpTable(tcpTable, &size, TRUE);
        if (ret == ERROR_INSUFFICIENT_BUFFER) {
            continue;
        }
        else if (ret == NO_ERROR) {
            for (DWORD i = 0; i < tcpTable->dwNumEntries; ++i) {
                connections.push_back(std::make_unique<TCP4Connection>(tcpTable->table[i]));
            }
            break;
        }
        else {
            throw std::runtime_error("Failed to retrieve TCP table");
        }
    }
    tcpTableBuffer = {};

    // Retrieve TCPv6 connections
    std::vector<uint8_t> tcp6TableBuffer;
    size = 0;
    while (GetTcp6Table(nullptr, &size, TRUE) == ERROR_INSUFFICIENT_BUFFER) {
        tcp6TableBuffer.resize(size);
        PMIB_TCP6TABLE tcp6Table = reinterpret_cast<PMIB_TCP6TABLE>(tcp6TableBuffer.data());
        auto ret = GetTcp6Table(tcp6Table, &size, TRUE);
        if (ret == ERROR_INSUFFICIENT_BUFFER) {
            continue;
        }
        else if (ret == NO_ERROR) {
            for (DWORD i = 0; i < tcp6Table->dwNumEntries; ++i) {
                connections.push_back(std::make_unique<TCP6Connection>(tcp6Table->table[i]));
            }
            break;
        }
        else {
            throw std::runtime_error("Failed to retrieve TCP6 table");
        }
    }
    tcp6TableBuffer = {};

    return connections;
}

