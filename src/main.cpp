#include "TCPConnection.h"

#include <ranges>
#include <chrono>
#include <functional>

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/thread.h>
#include <wx/clipbrd.h>

class MyFrame : public wxFrame
{
    wxTimer timer_;
    wxListCtrl* listCtrl_;

    std::vector<TCPConnectionPtr> connections_;
    using clock = std::chrono::steady_clock;
    std::optional<clock::time_point> lastRefreshTime_;
    std::vector<std::optional<TCPConnectionStatistics>> statistics_;

    void RefreshList() {
        using namespace std::ranges::views;

        try {
            auto all_connections = GetTCPConnections();

            auto filter_rtmp_target = [](auto& x) {
                return x->getRemotePort() == 1935;
            };
            auto filter_no_loopback = [&](auto& x) {
                auto ep = x->getRemoteEndpoint();
                for(auto& c : all_connections) {
                    if (!c) // 可能已经被move走了
                        continue;
                    if (c->getIpVersion() != x->getIpVersion()) {
                        continue;
                    }
                    if (c->getLocalEndpoint() == ep) {
                        return false;
                    }
                }
                return true;
            };

            auto streaming_connections = all_connections
                | filter(filter_rtmp_target)
                | filter(filter_no_loopback)
                | filter([](auto& x) { return x->enableStatistics(); })
                ;
            
            connections_.clear();
            for(auto& c: streaming_connections) {
                connections_.emplace_back(std::move(c));
            }

            lastRefreshTime_ = {};
            statistics_.clear();
            statistics_.resize(connections_.size());

            wxTheApp->CallAfter([this]() {
                listCtrl_->DeleteAllItems();
                for (auto& conn : connections_) {
                    long itemIndex = listCtrl_->InsertItem(listCtrl_->GetItemCount(), conn->getLocalAddress());
                    listCtrl_->SetItem(itemIndex, 1, conn->getRemoteAddress());
                    listCtrl_->SetItem(itemIndex, 2, L"取得中...");
                    listCtrl_->SetItem(itemIndex, 3, L"取得中...");
                    listCtrl_->SetItem(itemIndex, 4, L"取得中...");
                }
            });

            RefreshItems();
        }
        catch (const std::exception& e) {
            wxMessageBox(wxString::Format(L"再取得失敗しました: %s", e.what()), L"エラー", wxOK | wxICON_ERROR);
            return;
        }
    }

    void RefreshItems() {
        std::vector<std::function<void()>> update_tasks;

        for(auto i = 0; i < connections_.size(); ++i) {
            auto& conn = connections_[i];
            auto& stat = statistics_[i];

            auto curstat = conn->getStatistics();
            if (!curstat.has_value()) {
                update_tasks.emplace_back([=, this]() {
                    listCtrl_->SetItemBackgroundColour(i, *wxRED);
                });
                continue;
            }

            if (stat.has_value()) {
                auto time_diff = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - *lastRefreshTime_).count() / 1e9;
                auto rtt = curstat->rtt_ms;
                auto sent_diff = curstat->sent_bytes - stat->sent_bytes;
                auto recv_diff = curstat->received_bytes - stat->received_bytes;
                auto retrans_diff = curstat->retransmitted_bytes - stat->retransmitted_bytes;
                auto sent_mbps = sent_diff * 8 / 1e9 / time_diff;
                auto recv_mbps = recv_diff * 8 / 1e9 / time_diff;
                auto retrans_rate = static_cast<double>(retrans_diff) / sent_diff;

                update_tasks.emplace_back([=, this]() {
                    listCtrl_->SetItem(i, 2, wxString::Format(L"%d ms", rtt));
                    listCtrl_->SetItem(i, 3, wxString::Format(L"%.2lf Mbps", sent_mbps));
                    listCtrl_->SetItem(i, 4, wxString::Format(L"%.2lf %%", retrans_rate * 100));
                });
            }
            stat = curstat;
        }

        wxTheApp->CallAfter([tasks = std::move(update_tasks)]() mutable {
            for (auto& task : tasks) {
                task();
            }
        });

        lastRefreshTime_ = clock::now();
    }

public:
    MyFrame(wxWindow* parent): wxFrame(parent, wxID_ANY, L"配信回線情報 ——雷鳴 2025-04-30") {
        wxBoxSizer* rootsizer = new wxBoxSizer(wxVERTICAL);

        wxButton* refreshBtn = new wxButton(this, wxID_ANY, L"再取得");
        refreshBtn->SetToolTip(L"配信中でこのボタンを押すと、配信中の回線情報を再取得します。");
        rootsizer->Add(refreshBtn, 0, wxALL | wxEXPAND, 5);

        listCtrl_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
        listCtrl_->InsertColumn(0, L"自分のIPアドレス", wxLIST_FORMAT_LEFT, wxLIST_AUTOSIZE_USEHEADER);
        listCtrl_->InsertColumn(1, L"接続先のIPアドレス", wxLIST_FORMAT_LEFT, wxLIST_AUTOSIZE_USEHEADER);
        listCtrl_->InsertColumn(2, L"往復遅延時間", wxLIST_FORMAT_LEFT, wxLIST_AUTOSIZE_USEHEADER);
        listCtrl_->InsertColumn(3, L"送信速度", wxLIST_FORMAT_LEFT, wxLIST_AUTOSIZE_USEHEADER);
        listCtrl_->InsertColumn(4, L"再送信率", wxLIST_FORMAT_LEFT, wxLIST_AUTOSIZE_USEHEADER);

        listCtrl_->Bind(wxEVT_MOTION, [this](wxMouseEvent& event) {
            long col;
            int flags = 0;
            listCtrl_->HitTest(event.GetPosition(), flags, &col);

            if (col == 0) {
                listCtrl_->SetToolTip(L"自分のIPアドレス\r\n\r\n使用している回線（有線、無線、VPNなど）を確認できます。");
            } else if (col == 1) {
                listCtrl_->SetToolTip(L"接続先のIPアドレス\r\n\r\n接続先サーバーの情報を確認できます。\r\n【ダブルクリックでコピーが可能です】。\r\nまた、自分でインターネットを利用してサーバーの所在地を調べることもできます。");
            } else if (col == 2) {
                listCtrl_->SetToolTip(L"往復遅延時間（RTT）\r\n\r\n接続の往復遅延時間を表示します。\r\n値が高い場合、システムのDNSサーバー設定は間違えった可能性があります。\r\nまた、この値が高いほど、ネットワークがパケット損失に対して耐性が低い可能性があります。");
            } else if (col == 3) {
                listCtrl_->SetToolTip(L"送信速度\r\n\r\n配信中の音声や映像の送信速度を表示します。");
            } else if (col == 4) {
                listCtrl_->SetToolTip(L"再送信率\r\n\r\n送信データが途中で失われた場合の再送信率を表示します。");
            } else {
                listCtrl_->UnsetToolTip();
            }

            event.Skip();
        });

        listCtrl_->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
            int totalWidth = listCtrl_->GetClientSize().GetWidth();
            if (totalWidth < 100) // 否则会出现循环卡死，暂时不知道原因
                totalWidth = 100;
            listCtrl_->SetColumnWidth(0, totalWidth * 0.25); // "自分のアドレス"
            listCtrl_->SetColumnWidth(1, totalWidth * 0.25); // "接続先アドレス"
            listCtrl_->SetColumnWidth(2, totalWidth * 0.16); // "遅延"
            listCtrl_->SetColumnWidth(3, totalWidth * 0.17); // "上がる速さ"
            listCtrl_->SetColumnWidth(4, totalWidth * 0.16); // "再送信率"
            event.Skip();
        });

        listCtrl_->Bind(wxEVT_LEFT_DCLICK, [this](wxMouseEvent& event) {
            long itemIndex = listCtrl_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
            if (itemIndex != wxNOT_FOUND) {
                wxString text = listCtrl_->GetItemText(itemIndex, 1); // 第二个column
                if (wxTheClipboard->Open()) {
                    wxTheClipboard->SetData(new wxTextDataObject(text));
                    wxTheClipboard->Close();
                }
            }
            event.Skip();
        });

        rootsizer->Add(listCtrl_, 1, wxALL | wxEXPAND, 5);

        SetMinSize(wxSize(480, 240));
        SetSizerAndFit(rootsizer);

        refreshBtn->Bind(wxEVT_BUTTON, [=, this](wxCommandEvent&) {
            RefreshList();
        });
        timer_.Bind(wxEVT_TIMER, [=, this](wxTimerEvent&) {
            RefreshItems();
        });
        timer_.Start(1000); // 1秒ごとに更新
    }

    ~MyFrame() {
        timer_.Stop();
    }
};

class MyApp : public wxApp
{
public:
    bool OnInit() {
        MyFrame* frame = new MyFrame(nullptr);
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);
