#include <format>
#include <ctime>
#include <nlohmann/json.hpp>
#include "app_post_service.h"
#include "helpers/number_parser.h"

namespace SocialNetwork {

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



void PostService::register_endpoints(httplib::Server* server)
{
    if (!server) return;
    server->Get(R"(/post/get/([0-9a-fA-F-]{36}))", [this](const auto& req, auto& res) {
        LOG_DEBUG("handler: GET /post/get/:id");
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_get_id_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_get_id();
        if (!ok) metrics_->count_failed_request_post_get_id();
        if (ok)  metrics_->store_latency_request_post_get_id(std::chrono::duration<double>(end - start).count());
    })
    .Put(R"(/post/delete/([0-9a-fA-F-]{36}))", [this](const auto& req, auto& res) {
        LOG_DEBUG("handler: PUT /post/delete/:id");
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_delete_id_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_delete_id();
        if (!ok) metrics_->count_failed_request_post_delete_id();
        if (ok)  metrics_->store_latency_request_post_delete_id(std::chrono::duration<double>(end - start).count());
    })
    .Post("/post/create", [this](const auto& req, auto& res) {
        LOG_DEBUG("handler: POST /post/create");
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_create_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_create();
        if (!ok) metrics_->count_failed_request_post_create();
        if (ok)  metrics_->store_latency_request_post_create(std::chrono::duration<double>(end - start).count());
    })
    .Put("/post/update", [this](const auto& req, auto& res) {
        LOG_DEBUG("handler: PUT /post/update");
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_update_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_update();
        if (!ok) metrics_->count_failed_request_post_update();
        if (ok)  metrics_->store_latency_request_post_update(std::chrono::duration<double>(end - start).count());
    })
    .Get("/post/feed", [this](const auto& req, auto& res) {
        LOG_DEBUG("handler: GET /post/feed");
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_feed_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_feed();
        if (!ok) metrics_->count_failed_request_post_feed();
        if (ok)  metrics_->store_latency_request_post_feed(std::chrono::duration<double>(end - start).count());
    });
    LOG_INFOR("endpoints registered: GET /post/get/:id -- PUT /post/delete/:id -- POST /post/create -- PUT /post/update -- GET /post/feed");
}

bool PostService::pre_routing_validation(const httplib::Request& req)
{
    if (req.path.starts_with("/post/get/")
    ||  req.path.starts_with("/post/delete/")
    ||  req.path == "/post/create"
    ||  req.path == "/post/update"
    ||  req.path == "/post/feed") {
        return true;
    }
    return false;
}

bool PostService::post_get_id_handler(const httplib::Request& req, httplib::Response& res)
{
    const std::string requested_id = req.matches[1];
    if (!AuthService::is_valid_uuid(requested_id)) {
        LOG_ERROR(std::format("post_get_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("post_get_id_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("post_get_id_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
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
                res.status = httplib::StatusCode::NotFound_404;
                return false;
            } else {
                // успешное получение поста
                res.set_content(serialize_post(rv.post.value()), "application/json");
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("post_get_id_handler exception: {} (post_id: {})", ex.what(), post_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool PostService::post_delete_id_handler(const httplib::Request& req, httplib::Response& res)
{
    const std::string requested_id = req.matches[1];
    if (!AuthService::is_valid_uuid(requested_id)) {
        LOG_ERROR(std::format("post_delete_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("post_delete_id_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("post_delete_id_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    const std::string post_id{requested_id};
    const std::string user_id{id};

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
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("post_delete_id_handler exception: {} (post_id: {}, user_id: {})", ex.what(), post_id, user_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool PostService::post_create_handler(const httplib::Request& req, httplib::Response& res)
{
    auto body = nlohmann::json::parse(req.body);

    if (!body.contains("text")) {
        LOG_ERROR(std::format("post_create_handler: request params does not contain 'text'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!body["text"].is_string()) {
        LOG_ERROR(std::format("post_create_handler: request params 'text' should be a string"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("post_create_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("post_create_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    const std::string content{body["text"].get<std::string>()};
    const std::string user_id{id};

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
                res.set_content(j.dump(), "application/json");
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("post_create_handler exception: {} (user_id: {})", ex.what(), user_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool PostService::post_update_handler(const httplib::Request& req, httplib::Response& res)
{
    auto body = nlohmann::json::parse(req.body);

    if (!body.contains("id")
    ||  !body.contains("text")) {
        LOG_ERROR(std::format("post_update_handler: request params does not contain 'id' and/or 'text'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!body["id"].is_string()
    ||  !body["text"].is_string()) {
        LOG_ERROR(std::format("post_update_handler: request params 'id' and 'text' should be a string"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!AuthService::is_valid_uuid(body["id"].get<std::string>())) {
        LOG_ERROR(std::format("post_update_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("post_update_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("post_update_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    const std::string content{body["text"].get<std::string>()};
    const std::string post_id{body["id"].get<std::string>()};
    const std::string user_id{id};

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
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("post_update_handler exception: {} (post_id: {}, user_id: {})", ex.what(), post_id, user_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool PostService::post_feed_handler(const httplib::Request& req, httplib::Response& res)
{
    size_t offset = 0;
    size_t limit  = 10;

    if (req.has_param("offset")) {
        auto v = NumberParserHelpers::parse_int(req.get_param_value("offset"));
        if (v.has_value() && v.value() >= 0) {
            offset = static_cast<size_t>(v.value());
        }
    }
    if (req.has_param("limit")) {
        auto v = NumberParserHelpers::parse_int(req.get_param_value("limit"));
        if (v.has_value() && v.value() >= 1) {
            limit = static_cast<size_t>(v.value());
        }
    }

    // добавляем в ответ несколько заголовков с информацией о параметрах "текущей страницы"
    res.set_header("X-Pagination-Offset", std::to_string(offset));
    res.set_header("X-Pagination-Limit", std::to_string(limit));

    if (!db_ || !auth_) {
        auto err = std::format("post_feed_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("post_feed_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    const std::string user_id{id};

    std::string err{};
    try {
        if (cache_) {
            std::chrono::system_clock::time_point expires_at{};
            std::chrono::seconds                  remaining_ttl{};
            auto cache_feed = cache_->get_feed(user_id, expires_at, remaining_ttl);
            if (cache_feed.has_value()) {
                res.set_header("Cache-Control", std::format("max-age={}, must-revalidate", remaining_ttl.count()));
                res.set_header("X-Cache-Status", "HIT");
                res.set_header("X-Cache-Total-Count", std::to_string(cache_feed.value().size()));
                res.set_header("X-Cache-Expires", time_point_to_format_str_(expires_at));
                metrics_->count_feed_cache_hits();
            } else {
                res.set_header("X-Cache-Status", "MISS");
                metrics_->count_feed_cache_misses();
            }

            if (cache_feed.has_value()) {
                res.set_content(serialize_posts(get_page(cache_feed.value(), offset, limit)), "application/json");
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

            res.set_content(serialize_posts(get_page(rv.posts, offset, limit)), "application/json");
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("post_feed_handler exception: {} (user_id: {}, offset: {}, limit: {})", ex.what(), user_id, offset, limit);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        nlohmann::json j = {{"code", 500}, {"message", err}};
        res.set_content(j.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

std::vector<DatabaseService::Post> PostService::get_page(const std::vector<DatabaseService::Post>& feed, size_t offset, size_t limit)
{
    if (offset >= feed.size()) return {};

    auto start = feed.begin() + offset;
    auto end   = (offset + limit) < feed.size()
               ? (feed.begin() + offset + limit)
               : (feed.end());

    return std::vector<DatabaseService::Post>(start, end);
}

std::string PostService::serialize_posts(const std::vector<DatabaseService::Post>& posts)
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

std::string PostService::serialize_post(const DatabaseService::Post& post)
{
    nlohmann::json j = {{"id",              post.id},
                        {"author_user_id",  post.author_user_id},
                        {"text",            post.text}};
    return j.dump();
}

} // namespace SocialNetwork
