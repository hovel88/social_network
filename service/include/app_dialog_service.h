#pragma once

#include <httplib.h>
#include "logger/logger.h"
#include "app_metrics.h"
#include "app_database_service.h"
#include "app_auth_service.h"

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
                           std::shared_ptr<DatabaseService> db,
                           std::shared_ptr<AuthService> auth)
    :   logger_(std::move(logger)),
        metrics_(std::move(metrics)),
        db_(std::move(db)),
        auth_(std::move(auth)) {}

    void register_endpoints(httplib::Server* server);
    bool pre_routing_validation(const httplib::Request& req);

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Metrics>         metrics_{nullptr};
    std::shared_ptr<DatabaseService> db_{nullptr};
    std::shared_ptr<AuthService>     auth_{nullptr};

    bool dialog_send_handler(const httplib::Request& req, httplib::Response& res);
    bool dialog_list_handler(const httplib::Request& req, httplib::Response& res);

    static std::vector<DatabaseService::Message> get_page(const std::vector<DatabaseService::Message>& dialog, size_t offset, size_t limit);
    static std::string serialize_messages(const std::vector<DatabaseService::Message>& dialog);
};
