#include <format>
#include <ctime>
#include <nlohmann/json.hpp>
#include "app_friend_service.h"
#include "app_auth_service.h"

void FriendService::register_endpoints(drogon::HttpAppFramework* server)
{
    if (server == nullptr) return;

    server->registerHandlerViaRegex(R"(/friend/set/([0-9a-fA-F-]{36}))",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback, const std::string& requested_id) {
            LOGGER_DEBUG("handler: PUT /friend/set/:id");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = friend_set_id_handler(req, res, requested_id);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_friend_set_id();
            if (!ok) metrics_->count_failed_request_friend_set_id();
            if (ok)  metrics_->store_latency_request_friend_set_id(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Put, "MiddlewareAuth"});

    server->registerHandlerViaRegex(R"(/friend/delete/([0-9a-fA-F-]{36}))",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback, const std::string& requested_id) {
            LOGGER_DEBUG("handler: PUT /friend/delete/:id");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = friend_delete_id_handler(req, res, requested_id);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_friend_delete_id();
            if (!ok) metrics_->count_failed_request_friend_delete_id();
            if (ok)  metrics_->store_latency_request_friend_delete_id(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Put, "MiddlewareAuth"});
}

bool FriendService::friend_set_id_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id)
{
    const std::string user_id = req->attributes()->get<std::string>("user_id");

    if (!AuthService::is_valid_uuid(requested_id)) {
        LOGGER_ERROR(std::format("friend_set_id_handler: request param 'id' is not an UUID format"));
        return false;
    }

    if (user_id == requested_id) {
        LOGGER_ERROR(std::format("friend_set_id_handler: cannot add yourself to friends"));
        return false;
    }

    const std::string friend_id{requested_id};

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
            res->setStatusCode(drogon::HttpStatusCode::k200OK);
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("friend_set_id_handler exception: {} (user_id: {}, friend_id: {})", ex.what(), user_id, friend_id);
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

bool FriendService::friend_delete_id_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id)
{
    const std::string user_id = req->attributes()->get<std::string>("user_id");

    if (!AuthService::is_valid_uuid(requested_id)) {
        LOGGER_ERROR(std::format("friend_delete_id_handler: request param 'id' is not an UUID format"));
        return false;
    }

    if (user_id == requested_id) {
        LOGGER_ERROR(std::format("friend_delete_id_handler: cannot delete yourself from friends"));
        return false;
    }

    const std::string friend_id{requested_id};

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
            res->setStatusCode(drogon::HttpStatusCode::k200OK);
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("friend_delete_id_handler exception: {} (user_id: {}, friend_id: {})", ex.what(), user_id, friend_id);
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
