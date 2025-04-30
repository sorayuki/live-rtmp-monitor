#include "TCPConnection.h"

#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdexcept>

class TCPConnectionBase: public TCPConnection {
protected:
    int ipVersion_;
    std::string localAddress_;
    uint16_t localPort_;
    std::string localEndpoint_;
    std::string remoteAddress_;
    uint16_t remotePort_;
    std::string remoteEndpoint_;

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

    std::string getLocalEndpoint() const {
        return localEndpoint_;
    }

    std::string getRemoteAddress() const {
        return remoteAddress_;
    }

    uint16_t getRemotePort() const {
        return remotePort_;
    }

    std::string getRemoteEndpoint() const {
        return remoteEndpoint_;
    }
};


template<int ipVersion>
struct TCPTraits;

template<>
struct TCPTraits<4> {
    using RowType = MIB_TCPROW;
    using TableType = MIB_TCPTABLE;
    static constexpr auto GetTableFunc = GetTcpTable;
    static constexpr auto SetEStatsFunc = SetPerTcpConnectionEStats;
    static constexpr auto GetEStatsFunc = GetPerTcpConnectionEStats;
};

template<>
struct TCPTraits<6> {
    using RowType = MIB_TCP6ROW;
    using TableType = MIB_TCP6TABLE;
    static constexpr auto GetTableFunc = GetTcp6Table;
    static constexpr auto SetEStatsFunc = SetPerTcp6ConnectionEStats;
    static constexpr auto GetEStatsFunc = GetPerTcp6ConnectionEStats;
};


template<int ipVersion>
class TCPConnectionImpl : public TCPConnectionBase {
    typename TCPTraits<ipVersion>::RowType tcpRow_;
    bool statisticsEnabled_ = false;

    void disableStatistics() {
        TCP_ESTATS_DATA_RW_v0 data_rw = { FALSE };
        TCP_ESTATS_PATH_RW_v0 path_rw = { FALSE };
        TCPTraits<ipVersion>::SetEStatsFunc(&tcpRow_, TcpConnectionEstatsPath, reinterpret_cast<PUCHAR>(&path_rw), 0, sizeof(path_rw), 0);
        TCPTraits<ipVersion>::SetEStatsFunc(&tcpRow_, TcpConnectionEstatsData, reinterpret_cast<PUCHAR>(&data_rw), 0, sizeof(data_rw), 0);
    }

public:
    TCPConnectionImpl(const typename TCPTraits<ipVersion>::RowType& tcpRow)
        : tcpRow_(tcpRow) 
    {
        ipVersion_ = ipVersion;

        char addressBuffer[ipVersion == 4 ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN];
        if constexpr (ipVersion == 4) {
            inet_ntop(AF_INET, &tcpRow_.dwLocalAddr, addressBuffer, sizeof(addressBuffer));
        } else {
            inet_ntop(AF_INET6, &tcpRow_.LocalAddr, addressBuffer, sizeof(addressBuffer));
        }
        localAddress_ = std::string(addressBuffer);
        localPort_ = ntohs(static_cast<uint16_t>(tcpRow_.dwLocalPort & 0xFFFF));
        if constexpr (ipVersion == 4) {
            localEndpoint_ = localAddress_ + ":" + std::to_string(localPort_);
        } else {
            localEndpoint_ = "[" + localAddress_ + "]:" + std::to_string(localPort_);
        }

        if constexpr (ipVersion == 4) {
            inet_ntop(AF_INET, &tcpRow_.dwRemoteAddr, addressBuffer, sizeof(addressBuffer));
        } else {
            inet_ntop(AF_INET6, &tcpRow_.RemoteAddr, addressBuffer, sizeof(addressBuffer));
        }
        remoteAddress_ = std::string(addressBuffer);
        remotePort_ = ntohs(static_cast<uint16_t>(tcpRow_.dwRemotePort & 0xFFFF));
        if constexpr (ipVersion == 4) {
            remoteEndpoint_ = remoteAddress_ + ":" + std::to_string(remotePort_);
        } else {
            remoteEndpoint_ = "[" + remoteAddress_ + "]:" + std::to_string(remotePort_);
        }
    }

    ~TCPConnectionImpl() override {
        if (statisticsEnabled_) {
            disableStatistics();
        }
    }

    bool enableStatistics() override {
        TCP_ESTATS_DATA_RW_v0 data_rw = { TRUE };
        TCP_ESTATS_PATH_RW_v0 path_rw = { TRUE };
        if (TCPTraits<ipVersion>::SetEStatsFunc(&tcpRow_, TcpConnectionEstatsPath, reinterpret_cast<PUCHAR>(&path_rw), 0, sizeof(path_rw), 0) == NO_ERROR
            && TCPTraits<ipVersion>::SetEStatsFunc(&tcpRow_, TcpConnectionEstatsData, reinterpret_cast<PUCHAR>(&data_rw), 0, sizeof(data_rw), 0) == NO_ERROR
        ) {
            statisticsEnabled_ = true;
            return true;
        }
        disableStatistics();
        return false;
    }

    std::optional<TCPConnectionStatistics> getStatistics() override {
        if (!statisticsEnabled_)
            return {};
        
        TCP_ESTATS_PATH_ROD_v0 path_rod = { 0 };
        TCP_ESTATS_DATA_ROD_v0 data_rod = { 0 };
        if (TCPTraits<ipVersion>::GetEStatsFunc(&tcpRow_, TcpConnectionEstatsPath, 
                nullptr, 0, 0,
                nullptr, 0, 0, 
                reinterpret_cast<PUCHAR>(&path_rod), 0, sizeof(path_rod)
            ) == NO_ERROR
            && TCPTraits<ipVersion>::GetEStatsFunc(&tcpRow_, TcpConnectionEstatsData,
                nullptr, 0, 0,
                nullptr, 0, 0,
                reinterpret_cast<PUCHAR>(&data_rod), 0, sizeof(data_rod)
            ) == NO_ERROR
        ) {
            TCPConnectionStatistics stats;
            stats.rtt_ms = path_rod.SampleRtt;
            stats.sent_bytes = data_rod.DataBytesOut;
            stats.received_bytes = data_rod.DataBytesIn;
            stats.retransmitted_bytes = path_rod.BytesRetrans;
            return stats;
        }

        return {};
    }
};


template<int ipVersion>
void retrieveConnections(std::vector<TCPConnectionPtr>& connections) {
    using Traits = TCPTraits<ipVersion>;
    using TableType = typename Traits::TableType;
    using RowType = typename Traits::RowType;

    std::vector<uint8_t> tableBuffer;
    DWORD size = 0;
    while (Traits::GetTableFunc(nullptr, &size, TRUE) == ERROR_INSUFFICIENT_BUFFER) {
        size += sizeof(RowType) * 100;
        tableBuffer.resize(size);
        auto table = reinterpret_cast<TableType*>(tableBuffer.data());
        auto ret = Traits::GetTableFunc(table, &size, TRUE);
        if (ret == ERROR_INSUFFICIENT_BUFFER) {
            continue;
        } else if (ret == NO_ERROR) {
            for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                connections.push_back(std::make_unique<TCPConnectionImpl<ipVersion>>(table->table[i]));
            }
            break;
        } else {
            throw std::runtime_error("Failed to retrieve TCP table");
        }
    }
}


std::vector<TCPConnectionPtr> GetTCPConnections() {
    std::vector<TCPConnectionPtr> connections;
    retrieveConnections<4>(connections);
    retrieveConnections<6>(connections);
    return connections;
}

