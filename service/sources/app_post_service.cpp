#include <format>
#include <ctime>
#include <nlohmann/json.hpp>
#include "app_post_service.h"
#include "helpers/number_parser.h"

namespace SocialNetwork {

void PostService::register_endpoints(httplib::Server* server)
{
    if (!server) return;
    server->Get("/post/get/:id", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_get_id_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_get_id();
        if (!ok) metrics_->count_failed_request_post_get_id();
        if (ok)  metrics_->store_latency_request_post_get_id(std::chrono::duration<double>(end - start).count());
    })
    .Put("/post/delete/:id", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_delete_id_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_delete_id();
        if (!ok) metrics_->count_failed_request_post_delete_id();
        if (ok)  metrics_->store_latency_request_post_delete_id(std::chrono::duration<double>(end - start).count());
    })
    .Post("/post/create", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_create_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_create();
        if (!ok) metrics_->count_failed_request_post_create();
        if (ok)  metrics_->store_latency_request_post_create(std::chrono::duration<double>(end - start).count());
    })
    .Put("/post/update", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_update_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_update();
        if (!ok) metrics_->count_failed_request_post_update();
        if (ok)  metrics_->store_latency_request_post_update(std::chrono::duration<double>(end - start).count());
    })
    .Get("/post/feed", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = post_feed_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_post_feed();
        if (!ok) metrics_->count_failed_request_post_feed();
        if (ok)  metrics_->store_latency_request_post_feed(std::chrono::duration<double>(end - start).count());
    });
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
    nlohmann::json response{};

    if (!req.path_params.contains("id")) {
        LOG_ERROR(std::format("post_get_id_handler: request params does not contain 'id'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!AuthService::is_valid_uuid(req.path_params.at("id"))) {
        LOG_ERROR(std::format("post_get_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("post_get_id_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("post_get_id_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    const std::string post_id{req.path_params.at("id")};

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
                // успешное получение анкеты пользователя
                response = {{"id",              rv.post.value().id},
                            {"author_user_id",  rv.post.value().author_user_id},
                            {"text",            rv.post.value().text}};
                res.set_content(response.dump(), "application/json");
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("post_get_id_handler exception: {} (post_id: {})", ex.what(), post_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        response = {{"code", 500}, {"message", err}};
        res.set_content(response.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool PostService::post_delete_id_handler(const httplib::Request& req, httplib::Response& res)
{
    nlohmann::json response{};

    if (!req.path_params.contains("id")) {
        LOG_ERROR(std::format("post_delete_id_handler: request params does not contain 'id'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!AuthService::is_valid_uuid(req.path_params.at("id"))) {
        LOG_ERROR(std::format("post_delete_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("post_delete_id_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("post_delete_id_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    const std::string post_id{req.path_params.at("id")};
    const std::string user_id{id};

    std::string err{};
    try {
        auto rv = db_->delete_post(post_id, user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            // возвращаем просто 200 OK
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("post_delete_id_handler exception: {} (post_id: {}, user_id: {})", ex.what(), post_id, user_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        response = {{"code", 500}, {"message", err}};
        res.set_content(response.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool PostService::post_create_handler(const httplib::Request& req, httplib::Response& res)
{
    auto json = nlohmann::json::parse(req.body);
    nlohmann::json response{};

    if (!json.contains("text")) {
        LOG_ERROR(std::format("post_create_handler: request params does not contain 'text'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!json["text"].is_string()) {
        LOG_ERROR(std::format("post_create_handler: request params 'text' should be a string"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("post_create_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("post_create_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    const std::string content{json["text"].get<std::string>()};
    const std::string user_id{id};

    std::string err{};
    try {
        auto rv = db_->create_post(content, user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (rv.post_id.empty()) {
                err = std::format("can't register post");
            } else {
                // успешная регистрация поста
                response = {{"post_id", rv.post_id}};
                res.set_content(response.dump(), "application/json");
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("post_create_handler exception: {} (user_id: {})", ex.what(), user_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        response = {{"code", 500}, {"message", err}};
        res.set_content(response.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool PostService::post_update_handler(const httplib::Request& req, httplib::Response& res)
{
    auto json = nlohmann::json::parse(req.body);
    nlohmann::json response{};

    if (!json.contains("id")
    ||  !json.contains("text")) {
        LOG_ERROR(std::format("post_update_handler: request params does not contain 'id' and/or 'text'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!json["id"].is_string()
    ||  !json["text"].is_string()) {
        LOG_ERROR(std::format("post_update_handler: request params 'id' and 'text' should be a string"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!AuthService::is_valid_uuid(json["id"].get<std::string>())) {
        LOG_ERROR(std::format("post_update_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_ || !auth_) {
        auto err = std::format("post_update_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    std::string id;
    if (!auth_->authenticate(req, id)) {
        LOG_ERROR(std::format("post_update_handler: request from unauthorized user"));
        res.status = httplib::StatusCode::Unauthorized_401;
        return false;
    }

    const std::string content{json["text"].get<std::string>()};
    const std::string post_id{json["id"].get<std::string>()};
    const std::string user_id{id};

    std::string err{};
    try {
        auto rv = db_->update_post(post_id, content, user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            // возвращаем просто 200 OK
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("post_update_handler exception: {} (post_id: {}, user_id: {})", ex.what(), post_id, user_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        response = {{"code", 500}, {"message", err}};
        res.set_content(response.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool PostService::post_feed_handler(const httplib::Request& req, httplib::Response& res)
{
    nlohmann::json response = nlohmann::json::array({});
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

    if (!db_ || !auth_) {
        auto err = std::format("post_feed_handler: database service and/or authentication service are unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
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
        auto rv = db_->feed_post(user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (rv.posts.empty()) {
                response = nlohmann::json::array({});
            } else {
                for (const auto& p : rv.posts) {
                    response.push_back({{"id",              p.id},
                                        {"author_user_id",  p.author_user_id},
                                        {"text",            p.text}});
                }
            }
            res.set_content(response.dump(), "application/json");
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("post_feed_handler exception: {} (user_id: {}, offset: {}, limit: {})", ex.what(), user_id, offset, limit);
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
