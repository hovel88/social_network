#pragma once

#include <drogon/drogon.h>
#include "logger/logger.h"
#include "app_metrics.h"
#include "app_inmem_service.h"

class DialogService
{
public:
    ~DialogService() = default;
    DialogService() = delete;
    DialogService(const DialogService&) = delete;
    DialogService(DialogService&&) = delete;
    DialogService& operator=(const DialogService&) = delete;
    DialogService& operator=(DialogService&&) = delete;

    explicit DialogService(std::shared_ptr<Logging::Logger> logger,
                           std::shared_ptr<Metrics> metrics,
                           std::shared_ptr<InMemService> inmem)
    :   logger_(std::move(logger)),
        metrics_(std::move(metrics)),
        inmem_(std::move(inmem)) {}

    void register_endpoints(drogon::HttpAppFramework* server);

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Metrics>         metrics_{nullptr};
    std::shared_ptr<InMemService>    inmem_{nullptr};

    bool dialog_send_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id);
    bool dialog_list_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id);

    static std::vector<InMemService::Message> get_page(const std::vector<InMemService::Message>& dialog, size_t offset, size_t limit);
    static std::string serialize_messages(const std::vector<InMemService::Message>& dialog);
};
