#include <format>
#include <ctime>
#include <nlohmann/json.hpp>
#include "app_friend_service.h"

namespace SocialNetwork {

void FriendService::register_endpoints(httplib::Server* server)
{
    if (!server) return;
    server->Put("/friend/set/:id", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = friend_set_id_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_friend_set_id();
        if (!ok) metrics_->count_failed_request_friend_set_id();
        if (ok)  metrics_->store_latency_request_friend_set_id(std::chrono::duration<double>(end - start).count());
    })
    .Put("/friend/delete/:id", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = friend_delete_id_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_friend_delete_id();
        if (!ok) metrics_->count_failed_request_friend_delete_id();
        if (ok)  metrics_->store_latency_request_friend_delete_id(std::chrono::duration<double>(end - start).count());
    });
}

bool FriendService::pre_routing_validation(const httplib::Request& req)
{
    if (req.path.starts_with("/friend/set/")
    ||  req.path.starts_with("/friend/delete/")) {
        return true;
    }
    return false;
}

bool FriendService::friend_set_id_handler(const httplib::Request& req, httplib::Response& res)
{
    nlohmann::json response{};

    if (!req.path_params.contains("id")) {
        LOG_ERROR(std::format("friend_set_id_handler: request params does not contain 'id'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!AuthService::is_valid_uuid(req.path_params.at("id"))) {
        LOG_ERROR(std::format("friend_set_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("friend_set_id_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("friend_set_id_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    if (id == req.path_params.at("id")) {
        LOG_ERROR(std::format("friend_set_id_handler: cannot add yourself to friends"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    const std::string friend_id{req.path_params.at("id")};
    const std::string user_id{id};

    std::string err{};
    try {
        auto rv = db_->add_friend(user_id, friend_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            // возвращаем просто 200 OK
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("friend_set_id_handler exception: {} (user_id: {}, friend_id: {})", ex.what(), user_id, friend_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        response = {{"code", 500}, {"message", err}};
        res.set_content(response.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool FriendService::friend_delete_id_handler(const httplib::Request& req, httplib::Response& res)
{
    nlohmann::json response{};

    if (!req.path_params.contains("id")) {
        LOG_ERROR(std::format("friend_delete_id_handler: request params does not contain 'id'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!AuthService::is_valid_uuid(req.path_params.at("id"))) {
        LOG_ERROR(std::format("friend_delete_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("friend_delete_id_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("friend_delete_id_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    if (id == req.path_params.at("id")) {
        LOG_ERROR(std::format("friend_delete_id_handler: cannot delete yourself from friends"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    const std::string friend_id{req.path_params.at("id")};
    const std::string user_id{id};

    std::string err{};
    try {
        auto rv = db_->delete_friend(user_id, friend_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            // возвращаем просто 200 OK
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("friend_delete_id_handler exception: {} (user_id: {}, friend_id: {})", ex.what(), user_id, friend_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        response = {{"code", 500}, {"message", err}};
        res.set_content(response.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

} // namespace SocialNetwork
