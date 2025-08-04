#include <format>
#include <ctime>
#include <nlohmann/json.hpp>
#include "app_dialog_service.h"

void DialogService::register_endpoints(drogon::HttpAppFramework* server)
{
    if (server == nullptr) return;

    server->registerHandlerViaRegex(R"(/dialog/([0-9a-fA-F-]{36})/send)",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback, const std::string& requested_id) {
            LOGGER_DEBUG("handler: POST /dialog/:id/send");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = dialog_send_handler(req, res, requested_id);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_dialog_send();
            if (!ok) metrics_->count_failed_request_dialog_send();
            if (ok)  metrics_->store_latency_request_dialog_send(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Post, "MiddlewareAuth"});

    server->registerHandlerViaRegex(R"(/dialog/([0-9a-fA-F-]{36})/list)",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback, const std::string& requested_id) {
            LOGGER_DEBUG("handler: GET /dialog/:id/list");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = dialog_list_handler(req, res, requested_id);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_dialog_list();
            if (!ok) metrics_->count_failed_request_dialog_list();
            if (ok)  metrics_->store_latency_request_dialog_list(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Get, "MiddlewareAuth"});
}

bool DialogService::dialog_send_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id)
{
    const std::string user_id = req->attributes()->get<std::string>("user_id");
    auto body = nlohmann::json::parse(req->getBody());

    if (!body.contains("text")) {
        LOGGER_ERROR(std::format("dialog_send_handler: request params does not contain 'text'"));
        return false;
    }

    if (!body["text"].is_string()) {
        LOGGER_ERROR(std::format("dialog_send_handler: request params 'text' should be a string"));
        return false;
    }

    if (!AuthService::is_valid_uuid(requested_id)) {
        LOGGER_ERROR(std::format("dialog_send_handler: request param 'id' is not an UUID format"));
        return false;
    }

    if (user_id == requested_id) {
        LOGGER_ERROR(std::format("dialog_send_handler: cannot make dialog with yourself"));
        return false;
    }

    const std::string message{body["text"].get<std::string>()};
    const std::string to_user_id{requested_id};
    const std::string from_user_id{user_id};

    std::string err{};
    try {
        auto rv = db_->send_dialog_message(from_user_id, to_user_id, message);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            // возвращаем просто 200 OK
            res->setStatusCode(drogon::HttpStatusCode::k200OK);
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("dialog_send_handler exception: {} (from_user_id: {}, to_user_id: {})", ex.what(), from_user_id, to_user_id);
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        res->setBody(j.dump());
    }
    return false;
}

bool DialogService::dialog_list_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id)
{
    const std::string user_id = req->attributes()->get<std::string>("user_id");

    if (!AuthService::is_valid_uuid(requested_id)) {
        LOGGER_ERROR(std::format("dialog_list_handler: request param 'id' is not an UUID format"));
        return false;
    }

    if (user_id == requested_id) {
        LOGGER_ERROR(std::format("dialog_list_handler: cannot make dialog with yourself"));
        return false;
    }

    const std::string to_user_id{requested_id};
    const std::string from_user_id{user_id};

    std::string err{};
    try {
        auto rv = db_->list_dialog_messages(from_user_id, to_user_id, 100);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            res->setStatusCode(drogon::HttpStatusCode::k200OK);
            res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
            res->setBody(serialize_messages(get_page(rv.messages, /*offset=*/0, /*limit=*/100)));
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("dialog_list_handler exception: {} (from_user_id: {}, to_user_id: {})", ex.what(), from_user_id, to_user_id);
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        res->setBody(j.dump());
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
