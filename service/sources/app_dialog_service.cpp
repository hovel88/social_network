#include <format>
#include <ctime>
#include <nlohmann/json.hpp>
#include "app_dialog_service.h"

namespace SocialNetwork {

void DialogService::register_endpoints(httplib::Server* server)
{
    if (!server) return;
    server->Post(R"(/dialog/([0-9a-fA-F-]{36})/send)", [this](const auto& req, auto& res) {
        LOG_DEBUG("handler: POST /dialog/:id/send");
        auto start = std::chrono::steady_clock::now();
        bool ok    = dialog_send_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_dialog_send();
        if (!ok) metrics_->count_failed_request_dialog_send();
        if (ok)  metrics_->store_latency_request_dialog_send(std::chrono::duration<double>(end - start).count());
    })
    .Get(R"(/dialog/([0-9a-fA-F-]{36})/list)", [this](const auto& req, auto& res) {
        LOG_DEBUG("handler: GET /dialog/:id/list");
        auto start = std::chrono::steady_clock::now();
        bool ok    = dialog_list_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_dialog_list();
        if (!ok) metrics_->count_failed_request_dialog_list();
        if (ok)  metrics_->store_latency_request_dialog_list(std::chrono::duration<double>(end - start).count());
    });
    LOG_INFOR("endpoints registered: POST /dialog/:id/send -- GET /dialog/:id/list");
}

bool DialogService::pre_routing_validation(const httplib::Request& req)
{
    if (req.path.starts_with("/dialog/")) {
        return true;
    }
    return false;
}

bool DialogService::dialog_send_handler(const httplib::Request& req, httplib::Response& res)
{
    auto body = nlohmann::json::parse(req.body);

    if (!body.contains("text")) {
        LOG_ERROR(std::format("dialog_send_handler: request params does not contain 'text'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!body["text"].is_string()) {
        LOG_ERROR(std::format("dialog_send_handler: request params 'text' should be a string"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    const std::string requested_id = req.matches[1];
    if (!AuthService::is_valid_uuid(requested_id)) {
        LOG_ERROR(std::format("dialog_send_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("dialog_send_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("dialog_send_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    if (id == requested_id) {
        LOG_ERROR(std::format("dialog_send_handler: cannot make dialog with yourself"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    const std::string message{body["text"].get<std::string>()};
    const std::string to_user_id{requested_id};
    const std::string from_user_id{id};

    std::string err{};
    try {
        auto rv = db_->send_dialog_message(from_user_id, to_user_id, message);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            // возвращаем просто 200 OK
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("dialog_send_handler exception: {} (from_user_id: {}, to_user_id: {})", ex.what(), from_user_id, to_user_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool DialogService::dialog_list_handler(const httplib::Request& req, httplib::Response& res)
{
    const std::string requested_id = req.matches[1];
    if (!AuthService::is_valid_uuid(requested_id)) {
        LOG_ERROR(std::format("dialog_list_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("dialog_list_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("dialog_list_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    if (id == requested_id) {
        LOG_ERROR(std::format("dialog_list_handler: cannot make dialog with yourself"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    const std::string to_user_id{requested_id};
    const std::string from_user_id{id};

    std::string err{};
    try {
        auto rv = db_->list_dialog_messages(from_user_id, to_user_id, 100);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            res.set_content(serialize_messages(get_page(rv.messages, /*offset=*/0, /*limit=*/100)), "application/json");
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("dialog_list_handler exception: {} (from_user_id: {}, to_user_id: {})", ex.what(), from_user_id, to_user_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

std::vector<DatabaseService::Message> DialogService::get_page(const std::vector<DatabaseService::Message>& dialog, size_t offset, size_t limit)
{
    if (offset >= dialog.size()) return {};

    auto start = dialog.begin() + offset;
    auto end   = (offset + limit) < dialog.size()
               ? (dialog.begin() + offset + limit)
               : (dialog.end());

    return std::vector<DatabaseService::Message>(start, end);
}

std::string DialogService::serialize_messages(const std::vector<DatabaseService::Message>& dialog)
{
    nlohmann::json j{};
    if (dialog.empty()) {
        j = nlohmann::json::array({});
    } else {
        for (const auto& m : dialog) {
            j.push_back({//{"created_at", m.created_at_msec},
                         {"from",       m.from},
                         {"to",         m.to},
                         {"text",       m.text}});
        }
    }
    return j.dump();
}

} // namespace SocialNetwork
