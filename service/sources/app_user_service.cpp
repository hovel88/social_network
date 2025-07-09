#include <ctime>
#include <iostream>
#include <nlohmann/json.hpp>
#include <bcrypt/BCrypt.hpp>
#include "app_user_service.h"
#include "app_auth_service.h"

namespace SocialNetwork {

void UserService::register_endpoints(httplib::Server* server)
{
    if (!server) return;
    server->Post("/login", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = login_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_login();
        if (!ok) metrics_->count_failed_request_login();
        if (ok)  metrics_->store_latency_request_login(std::chrono::duration<double>(end - start).count());
    })
    .Post("/user/register", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = user_register_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_user_register();
        if (!ok) metrics_->count_failed_request_user_register();
        if (ok)  metrics_->store_latency_request_user_register(std::chrono::duration<double>(end - start).count());
    })
    .Get("/user/get/:id", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = user_get_id_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_user_get_id();
        if (!ok) metrics_->count_failed_request_user_get_id();
        if (ok)  metrics_->store_latency_request_user_get_id(std::chrono::duration<double>(end - start).count());
    })
    .Get("/user/search", [this](const auto& req, auto& res) {
        auto start = std::chrono::steady_clock::now();
        bool ok    = user_search_handler(req, res);
        auto end   = std::chrono::steady_clock::now();
        metrics_->count_request_user_search();
        if (!ok) metrics_->count_failed_request_user_search();
        if (ok)  metrics_->store_latency_request_user_search(std::chrono::duration<double>(end - start).count());
    });
}

bool UserService::pre_routing_validation(const httplib::Request& req)
{
    if (req.path.starts_with("/user/get/")
    ||  req.path == "/user/register"
    ||  req.path == "/user/search"
    ||  req.path == "/login") {
        return true;
    }
    return false;
}

bool UserService::login_handler(const httplib::Request& req, httplib::Response& res)
{
    auto json = nlohmann::json::parse(req.body);
    nlohmann::json response{};

    if (!json.contains("id")
    ||  !json.contains("password")) {
        LOG_ERROR(std::format("login_handler: request params does not contain 'id' and/or 'password'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!json["id"].is_string()
    ||  !json["password"].is_string()) {
        LOG_ERROR(std::format("login_handler: request params 'id' and 'password' should be a string"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!AuthService::is_valid_uuid(json["id"].get<std::string>())) {
        LOG_ERROR(std::format("login_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_) {
        auto err = std::format("login_handler: database service is unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    const std::string user_id{json["id"].get<std::string>()};
    const std::string user_pwd{json["password"].get<std::string>()};

    std::string err{};
    try {
        auto rv = db_->login_user(user_id, user_pwd);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (rv.token.empty()) {
                // пользователь не найден
                res.status = httplib::StatusCode::NotFound_404;
                return false;
            } else {
                response = {{"token", rv.token}};
                res.set_content(response.dump(), "application/json");
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("login_handler exception: {} (user_id: {}, user_pwd: {})", ex.what(), user_id, std::string(user_pwd.size(), '*'));
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        response = {{"code", 500}, {"message", err}};
        res.set_content(response.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool UserService::user_register_handler(const httplib::Request& req, httplib::Response& res)
{
    auto json = nlohmann::json::parse(req.body);
    nlohmann::json response{};

    if (!json.contains("password")
    ||  !json.contains("first_name")
    ||  !json.contains("second_name")
    ||  !json.contains("birthdate")
    ||  !json.contains("biography")
    ||  !json.contains("city")) {
        LOG_ERROR(std::format("user_register_handler: request params does not contain 'password', 'first_name', 'second_name', 'birthdate', 'biography' and/or 'city'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!json["password"].is_string()
    ||  !json["first_name"].is_string()
    ||  !json["second_name"].is_string()
    ||  !json["birthdate"].is_string()
    ||  !json["biography"].is_string()
    ||  !json["city"].is_string()) {
        LOG_ERROR(std::format("user_register_handler: request params 'password', 'first_name', 'second_name', 'birthdate', 'biography' and 'city' should be a string"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (json["password"].get<std::string>().length() < 8) {
        // минимальная длина 8 символов
        LOG_ERROR(std::format("user_register_handler: request param 'password' should contain at least 8 characters"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    {
        std::tm t{};
        std::istringstream ss(json["birthdate"].get<std::string>());
        ss >> std::get_time(&t, "%Y-%m-%d"); // 2017-02-01
        if (ss.fail() || t.tm_year < 0 || t.tm_year > 107) {
            // 18лет (с 2007 г.д.), 2007 - 1900 = 107
            LOG_ERROR(std::format("user_register_handler: request param 'birthdate' is invalid"));
            res.status = httplib::StatusCode::BadRequest_400;
            return false;
        }
    }

    if (!db_) {
        auto err = std::format("user_register_handler: database service is unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    const std::string pwd{json["password"].get<std::string>()};
    const std::string fname{json["first_name"].get<std::string>()};
    const std::string sname{json["second_name"].get<std::string>()};
    const std::string bdate{json["birthdate"].get<std::string>()};
    const std::string bio{json["biography"].get<std::string>()};
    const std::string city{json["city"].get<std::string>()};

    std::string err{};
    try {
        auto rv = db_->register_user(fname, sname, bdate, bio, city, pwd);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (rv.user_id.empty()) {
                err = std::format("can't register user '{} {}'", fname, sname);
            } else {
                // успешная регистрация пользователя
                response = {{"user_id", rv.user_id}};
                res.set_content(response.dump(), "application/json");
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("user_register_handler exception: {} (fname: {}, sname: {}, bdate: {}, city: {}, pwd: {})", ex.what(), fname, sname, bdate, city, std::string(pwd.size(), '*'));
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        response = {{"code", 500}, {"message", err}};
        res.set_content(response.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool UserService::user_get_id_handler(const httplib::Request& req, httplib::Response& res)
{
    nlohmann::json response{};

    if (!req.path_params.contains("id")) {
        LOG_ERROR(std::format("user_get_id_handler: request params does not contain 'id'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!AuthService::is_valid_uuid(req.path_params.at("id"))) {
        LOG_ERROR(std::format("user_get_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_) {
        auto err = std::format("user_get_id_handler: database service is unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    const std::string user_id{req.path_params.at("id")};

    std::string err{};
    try {
        auto rv = db_->get_user(user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (!rv.user.has_value()) {
                // анкета не найдена
                res.status = httplib::StatusCode::NotFound_404;
                return false;
            } else {
                // успешное получение анкеты пользователя
                response = {{"id",          rv.user.value().id},
                            {"first_name",  rv.user.value().first_name},
                            {"second_name", rv.user.value().second_name},
                            {"birthdate",   rv.user.value().birthdate},
                            {"biography",   rv.user.value().biography},
                            {"city",        rv.user.value().city}};
                res.set_content(response.dump(), "application/json");
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("user_get_id_handler exception: {} (user_id: {})", ex.what(), user_id);
    }

    if (!err.empty()) {
        LOG_ERROR(err);
        response = {{"code", 500}, {"message", err}};
        res.set_content(response.dump(), "application/json");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
    return false;
}

bool UserService::user_search_handler(const httplib::Request& req, httplib::Response& res)
{
    nlohmann::json response{};

    if (!req.has_param("first_name")
    ||  !req.has_param("last_name")) {
        LOG_ERROR(std::format("user_search_handler: request params does not contain 'first_name' and/or 'last_name'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_) {
        auto err = std::format("user_search_handler: database service is unavailable");
        LOG_ERROR(err);
        response = {{"code", 503}, {"message", err}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    const std::string fname{req.get_param_value("first_name") + "%"};
    const std::string sname{req.get_param_value("last_name") + "%"};

    std::string err{};
    try {
        auto rv = db_->search_user(fname, sname);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (rv.users.empty()) {
                response = nlohmann::json::array({});
            } else {
                for (const auto& u : rv.users) {
                    response.push_back({{"id",          u.id},
                                        {"first_name",  u.first_name},
                                        {"second_name", u.second_name},
                                        {"birthdate",   u.birthdate},
                                        {"biography",   u.biography},
                                        {"city",        u.city}});
                }
            }
            res.set_content(response.dump(), "application/json");
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("user_search_handler exception: {} (fname: '{}', sname: '{}')", ex.what(), fname, sname);
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
