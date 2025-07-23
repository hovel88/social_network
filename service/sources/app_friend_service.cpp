#include <format>
#include <ctime>
#include <nlohmann/json.hpp>
#include "app_friend_service.h"

void FriendService::register_endpoints(httplib::Server* server)
{
    if (!server) return;
    server->Put(R"(/friend/set/([0-9a-fA-F-]{36}))", [this](const auto& req, auto& res) {
        LOG_DEBUG("handler: PUT /friend/set/:id");
        auto start = std::chrono::steady_clock::now();
        bool ok    = friend_set_id_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_friend_set_id();
        if (!ok) metrics_->count_failed_request_friend_set_id();
        if (ok)  metrics_->store_latency_request_friend_set_id(std::chrono::duration<double>(end - start).count());
    })
    .Put(R"(/friend/delete/([0-9a-fA-F-]{36}))", [this](const auto& req, auto& res) {
        LOG_DEBUG("handler: PUT /friend/delete/:id");
        auto start = std::chrono::steady_clock::now();
        bool ok    = friend_delete_id_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_friend_delete_id();
        if (!ok) metrics_->count_failed_request_friend_delete_id();
        if (ok)  metrics_->store_latency_request_friend_delete_id(std::chrono::duration<double>(end - start).count());
    });
    LOG_INFOR("endpoints registered: PUT /friend/set/:id -- PUT /friend/delete/:id");
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
    const std::string requested_id = req.matches[1];
    if (!AuthService::is_valid_uuid(requested_id)) {
        LOG_ERROR(std::format("friend_set_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("friend_set_id_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("friend_set_id_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    if (id == requested_id) {
        LOG_ERROR(std::format("friend_set_id_handler: cannot add yourself to friends"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    const std::string friend_id{requested_id};
    const std::string user_id{id};

    std::string err{};
    try {
        auto rv = db_->add_friend(user_id, friend_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            // if (cache_) {
            //     // после добавления в друзья "прогреваем" кеш ленты постов в соответствии с новым другом
            //     auto user_p = db_->feed_post(user_id, 500);
            //     for (const auto& post : user_p.posts) {
            //         cache_->add_post_to_feed(friend_id, post);
            //     }
            //     auto friend_p = db_->feed_post(friend_id, 500);
            //     for (const auto& post : friend_p.posts) {
            //         cache_->add_post_to_feed(user_id, post);
            //     }
            // }

            // возвращаем просто 200 OK
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("friend_set_id_handler exception: {} (user_id: {}, friend_id: {})", ex.what(), user_id, friend_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool FriendService::friend_delete_id_handler(const httplib::Request& req, httplib::Response& res)
{
    const std::string requested_id = req.matches[1];
    if (!AuthService::is_valid_uuid(requested_id)) {
        LOG_ERROR(std::format("friend_delete_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("friend_delete_id_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("friend_delete_id_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    if (id == requested_id) {
        LOG_ERROR(std::format("friend_delete_id_handler: cannot delete yourself from friends"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    const std::string friend_id{requested_id};
    const std::string user_id{id};

    std::string err{};
    try {
        auto rv = db_->delete_friend(user_id, friend_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (cache_) {
                // связь по дружбе у нас двусторонняя.
                // поэтому делаем инвалидацию кеша для обоих пользователей
                cache_->invalidate_user(user_id);
                cache_->invalidate_user(friend_id);
            }

            // возвращаем просто 200 OK
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("friend_delete_id_handler exception: {} (user_id: {}, friend_id: {})", ex.what(), user_id, friend_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}
