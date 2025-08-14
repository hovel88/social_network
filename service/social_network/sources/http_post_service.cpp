#include <format>
#include <ctime>
#include <nlohmann/json.hpp>
#include "http_post_service.h"
#include "app_auth_service.h"

static std::string time_point_to_format_str_(const std::chrono::system_clock::time_point& p)
{
    std::time_t t = std::chrono::system_clock::to_time_t(p);
    std::tm tm_time;
    gmtime_r(&t, &tm_time);

    std::stringstream ss;
    //                             2025-07-09T17:49:00Z
    ss << std::put_time(&tm_time, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

//-----------------------------------------------------------------------------



void HttpPostService::register_endpoints(drogon::HttpAppFramework* server)
{
    if (server == nullptr) return;

    server->registerHandlerViaRegex(R"(/post/get/([0-9a-fA-F-]{36}))",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback, const std::string& requested_id) {
            LOGGER_DEBUG("handler: GET /post/get/:id");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = post_get_id_handler(req, res, requested_id);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_post_get_id();
            if (!ok) metrics_->count_failed_request_post_get_id();
            if (ok)  metrics_->store_latency_request_post_get_id(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Get, "HttpMiddlewareAuth"});

    server->registerHandlerViaRegex(R"(/post/delete/([0-9a-fA-F-]{36}))",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback, const std::string& requested_id) {
            LOGGER_DEBUG("handler: PUT /post/delete/:id");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = post_delete_id_handler(req, res, requested_id);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_post_delete_id();
            if (!ok) metrics_->count_failed_request_post_delete_id();
            if (ok)  metrics_->store_latency_request_post_delete_id(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Put, "HttpMiddlewareAuth"});

    server->registerHandler("/post/create",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback) {
            LOGGER_DEBUG("handler: POST /post/create");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = post_create_handler(req, res);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_post_create();
            if (!ok) metrics_->count_failed_request_post_create();
            if (ok)  metrics_->store_latency_request_post_create(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Post, "HttpMiddlewareAuth"});

    server->registerHandler("/post/update",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback) {
            LOGGER_DEBUG("handler: PUT /post/update");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = post_update_handler(req, res);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_post_update();
            if (!ok) metrics_->count_failed_request_post_update();
            if (ok)  metrics_->store_latency_request_post_update(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Put, "HttpMiddlewareAuth"});

    server->registerHandler("/post/feed",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback) {
            LOGGER_DEBUG("handler: GET /post/feed");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = post_feed_handler(req, res);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_post_feed();
            if (!ok) metrics_->count_failed_request_post_feed();
            if (ok)  metrics_->store_latency_request_post_feed(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Get, "HttpMiddlewareAuth"});
}

bool HttpPostService::post_get_id_handler(const drogon::HttpRequestPtr& /*req*/, drogon::HttpResponsePtr& res, const std::string& requested_id)
{
    // const std::string user_id = req->attributes()->get<std::string>("user_id");

    if (!AuthService::is_valid_uuid(requested_id)) {
        LOGGER_ERROR(std::format("post_get_id_handler: request param 'id' is not an UUID format"));
        return false;
    }

    const std::string post_id{requested_id};

    std::string err{};
    try {
        auto rv = db_->get_post(post_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (!rv.post.has_value()) {
                // пост не найден
                res->setStatusCode(drogon::HttpStatusCode::k404NotFound);
                return false;
            } else {
                // успешное получение поста
                res->setStatusCode(drogon::HttpStatusCode::k200OK);
                res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                res->setBody(serialize_post(rv.post.value()));
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("post_get_id_handler exception: {} (post_id: {})", ex.what(), post_id);
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

bool HttpPostService::post_delete_id_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res, const std::string& requested_id)
{
    const std::string user_id = req->attributes()->get<std::string>("user_id");

    if (!AuthService::is_valid_uuid(requested_id)) {
        LOGGER_ERROR(std::format("post_delete_id_handler: request param 'id' is not an UUID format"));
        return false;
    }

    const std::string post_id{requested_id};

    std::string err{};
    try {
        auto rv = db_->delete_post(post_id, user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (cache_) {
                // полная инвалидация кеша всех друзей
                auto friends = db_->get_friends(user_id);
                for (const auto& friend_id : friends.friend_ids) {
                    cache_->invalidate_user(friend_id);
                }
            }

            // возвращаем просто 200 OK
            res->setStatusCode(drogon::HttpStatusCode::k200OK);
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("post_delete_id_handler exception: {} (post_id: {}, user_id: {})", ex.what(), post_id, user_id);
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

bool HttpPostService::post_create_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res)
{
    const std::string user_id = req->attributes()->get<std::string>("user_id");
    auto body = nlohmann::json::parse(req->getBody());

    if (!body.contains("text")) {
        LOGGER_ERROR(std::format("post_create_handler: request params does not contain 'text'"));
        return false;
    }

    if (!body["text"].is_string()) {
        LOGGER_ERROR(std::format("post_create_handler: request params 'text' should be a string"));
        return false;
    }

    const std::string content{body["text"].get<std::string>()};

    std::string err{};
    try {
        auto rv = db_->create_post(content, user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (!rv.post.has_value()) {
                err = std::format("can't register post");
            } else {
                if (cache_) {
                    // делаем так, чтобы новый пост появился в лентах друзей
                    auto friends = db_->get_friends(user_id);
                    for (const auto& friend_id : friends.friend_ids) {
                        cache_->add_post_to_feed(friend_id, rv.post.value());
                    }
                }

                // успешная регистрация поста
                nlohmann::json j = {{"post_id", rv.post.value().id}};
                res->setStatusCode(drogon::HttpStatusCode::k200OK);
                res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                res->setBody(j.dump());
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("post_create_handler exception: {} (user_id: {})", ex.what(), user_id);
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

bool HttpPostService::post_update_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res)
{
    const std::string user_id = req->attributes()->get<std::string>("user_id");
    auto body = nlohmann::json::parse(req->getBody());

    if (!body.contains("id")
    ||  !body.contains("text")) {
        LOGGER_ERROR(std::format("post_update_handler: request params does not contain 'id' and/or 'text'"));
        return false;
    }

    if (!body["id"].is_string()
    ||  !body["text"].is_string()) {
        LOGGER_ERROR(std::format("post_update_handler: request params 'id' and 'text' should be a string"));
        return false;
    }

    if (!AuthService::is_valid_uuid(body["id"].get<std::string>())) {
        LOGGER_ERROR(std::format("post_update_handler: request param 'id' is not an UUID format"));
        return false;
    }

    const std::string content{body["text"].get<std::string>()};
    const std::string post_id{body["id"].get<std::string>()};

    std::string err{};
    try {
        auto rv = db_->update_post(post_id, content, user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (cache_) {
                // полная инвалидация кеша всех друзей
                auto friends = db_->get_friends(user_id);
                for (const auto& friend_id : friends.friend_ids) {
                    cache_->invalidate_user(friend_id);
                }
            }

            // возвращаем просто 200 OK
            res->setStatusCode(drogon::HttpStatusCode::k200OK);
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("post_update_handler exception: {} (post_id: {}, user_id: {})", ex.what(), post_id, user_id);
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

bool HttpPostService::post_feed_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res)
{
    const std::string user_id = req->attributes()->get<std::string>("user_id");
    size_t offset = 0;
    size_t limit  = 10;

    {
        auto v = req->getOptionalParameter<size_t>("offset");
        if (v.has_value() /*&& v.value() >= 0*/) {
            offset = static_cast<size_t>(v.value());
        }
    }
    {
        auto v = req->getOptionalParameter<size_t>("limit");
        if (v.has_value() && v.value() >= 1) {
            limit = static_cast<size_t>(v.value());
        }
    }

    // добавляем в ответ несколько заголовков с информацией о параметрах "текущей страницы"
    res->addHeader("X-Pagination-Offset", std::to_string(offset));
    res->addHeader("X-Pagination-Limit", std::to_string(limit));

    std::string err{};
    try {
        if (cache_) {
            std::chrono::system_clock::time_point expires_at{};
            std::chrono::seconds                  remaining_ttl{};
            auto cache_feed = cache_->get_feed(user_id, expires_at, remaining_ttl);
            if (cache_feed.has_value()) {
                res->addHeader("Cache-Control", std::format("max-age={}, must-revalidate", remaining_ttl.count()));
                res->addHeader("X-Cache-Status", "HIT");
                res->addHeader("X-Cache-Total-Count", std::to_string(cache_feed.value().size()));
                res->addHeader("X-Cache-Expires", time_point_to_format_str_(expires_at));
                metrics_->count_feed_cache_hits();
            } else {
                res->addHeader("X-Cache-Status", "MISS");
                metrics_->count_feed_cache_misses();
            }

            if (cache_feed.has_value()) {
                res->setStatusCode(drogon::HttpStatusCode::k200OK);
                res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                res->setBody(serialize_posts(get_page(cache_feed.value(), offset, limit)));
                return true;
            }
        }

        auto rv = db_->feed_post(user_id, 1000);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (cache_) {
                cache_->put_feed(user_id, rv.posts);
            }

            res->setStatusCode(drogon::HttpStatusCode::k200OK);
            res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
            res->setBody(serialize_posts(get_page(rv.posts, offset, limit)));
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("post_feed_handler exception: {} (user_id: {}, offset: {}, limit: {})", ex.what(), user_id, offset, limit);
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

std::vector<DatabaseService::Post> HttpPostService::get_page(const std::vector<DatabaseService::Post>& feed, size_t offset, size_t limit)
{
    if (offset >= feed.size()) return {};

    auto start = feed.begin() + offset;
    auto end   = (offset + limit) < feed.size()
               ? (feed.begin() + offset + limit)
               : (feed.end());

    return std::vector<DatabaseService::Post>(start, end);
}

std::string HttpPostService::serialize_posts(const std::vector<DatabaseService::Post>& posts)
{
    nlohmann::json j{};
    if (posts.empty()) {
        j = nlohmann::json::array({});
    } else {
        for (const auto& p : posts) {
            j.push_back({{"id",              p.id},
                         {"author_user_id",  p.author_user_id},
                         {"text",            p.text}});
        }
    }
    return j.dump();
}

std::string HttpPostService::serialize_post(const DatabaseService::Post& post)
{
    nlohmann::json j = {{"id",              post.id},
                        {"author_user_id",  post.author_user_id},
                        {"text",            post.text}};
    return j.dump();
}
