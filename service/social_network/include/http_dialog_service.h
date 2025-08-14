#pragma once

#include <drogon/drogon.h>
#include <grpcpp/grpcpp.h>
#include <chat_service.grpc.pb.h>
#include "logger/logger.h"
#include "app_metrics.h"

class HttpDialogService
{
private:
    struct Message {
        std::string from{};
        std::string to{};
        std::string text{};
    };

public:
    ~HttpDialogService() = default;
    HttpDialogService() = delete;
    HttpDialogService(const HttpDialogService&) = delete;
    HttpDialogService(HttpDialogService&&) = delete;
    HttpDialogService& operator=(const HttpDialogService&) = delete;
    HttpDialogService& operator=(HttpDialogService&&) = delete;

    explicit HttpDialogService(std::shared_ptr<Logging::Logger> logger,
                               std::shared_ptr<Metrics> metrics,
                               std::shared_ptr<grpc::Channel> channel)
    :   logger_(std::move(logger)),
        metrics_(std::move(metrics)),
        stub_(social_network::chats::DialogService::NewStub(std::move(channel))) {}

    void register_endpoints(drogon::HttpAppFramework* server);

private:
    std::shared_ptr<Logging::Logger>                            logger_{nullptr};
    std::shared_ptr<Metrics>                                    metrics_{nullptr};
    std::unique_ptr<social_network::chats::DialogService::Stub> stub_{nullptr};

    bool dialog_send_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id);
    bool dialog_list_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id);

    static std::vector<Message> get_page(const std::vector<Message>& dialog, size_t offset, size_t limit);
    static std::string serialize_messages(const std::vector<Message>& dialog);
};
