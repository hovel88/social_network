#include <ctime>
#include <iostream>
#include <nlohmann/json.hpp>
#include <bcrypt/BCrypt.hpp>
#include "http_user_service.h"
#include "app_auth_service.h"

void HttpUserService::register_endpoints(drogon::HttpAppFramework* server)
{
    if (server == nullptr) return;

    server->registerHandler("/login",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback) {
            LOGGER_DEBUG("handler: POST /login");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = login_handler(req, res);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_login();
            if (!ok) metrics_->count_failed_request_login();
            if (ok)  metrics_->store_latency_request_login(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Post});

    server->registerHandler("/user/register",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback) {
            LOGGER_DEBUG("handler: POST /user/register");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = user_register_handler(req, res);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_user_register();
            if (!ok) metrics_->count_failed_request_user_register();
            if (ok)  metrics_->store_latency_request_user_register(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Post});

    server->registerHandlerViaRegex("/user/get/([0-9a-fA-F-]{36})",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback, const std::string& requested_id) {
            LOGGER_DEBUG("handler: GET /user/get/:id");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = user_get_id_handler(req, res, requested_id);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_user_get_id();
            if (!ok) metrics_->count_failed_request_user_get_id();
            if (ok)  metrics_->store_latency_request_user_get_id(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Get});

    server->registerHandler("/user/search",
        [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr&)>&& callback) {
            LOGGER_DEBUG("handler: GET /user/search");
            auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

            auto start = std::chrono::steady_clock::now();
            bool ok    = user_search_handler(req, res);
            auto end   = std::chrono::steady_clock::now();
            metrics_->count_request_user_search();
            if (!ok) metrics_->count_failed_request_user_search();
            if (ok)  metrics_->store_latency_request_user_search(std::chrono::duration<double>(end - start).count());

            callback(res);
        },
        {drogon::HttpMethod::Get});
}

bool HttpUserService::login_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res)
{
    auto body = nlohmann::json::parse(req->getBody());

    if (!body.contains("id")
    ||  !body.contains("password")) {
        LOGGER_ERROR(std::format("login_handler: request params does not contain 'id' and/or 'password'"));
        return false;
    }

    if (!body["id"].is_string()
    ||  !body["password"].is_string()) {
        LOGGER_ERROR(std::format("login_handler: request params 'id' and 'password' should be a string"));
        return false;
    }

    if (!AuthService::is_valid_uuid(body["id"].get<std::string>())) {
        LOGGER_ERROR(std::format("login_handler: request param 'id' is not an UUID format"));
        return false;
    }

    if (!db_) {
        auto err = std::format("login_handler: database service is unavailable");
        LOGGER_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res->setStatusCode(drogon::HttpStatusCode::k503ServiceUnavailable);
        res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        res->setBody(j.dump());
        return false;
    }

    const std::string user_id{body["id"].get<std::string>()};
    const std::string user_pwd{body["password"].get<std::string>()};

    std::string err{};
    try {
        auto rv = db_->login_user(user_id, user_pwd);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (rv.token.empty()) {
                // пользователь не найден
                res->setStatusCode(drogon::HttpStatusCode::k404NotFound);
                return false;
            } else {
                nlohmann::json j = {{"token", rv.token}};
                res->setStatusCode(drogon::HttpStatusCode::k200OK);
                res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                res->setBody(j.dump());
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("login_handler exception: {} (user_id: {}, user_pwd: {})", ex.what(), user_id, std::string(user_pwd.size(), '*'));
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

bool HttpUserService::user_register_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res)
{
    auto body = nlohmann::json::parse(req->getBody());

    if (!body.contains("password")
    ||  !body.contains("first_name")
    ||  !body.contains("second_name")
    ||  !body.contains("birthdate")
    ||  !body.contains("biography")
    ||  !body.contains("city")) {
        LOGGER_ERROR(std::format("user_register_handler: request params does not contain 'password', 'first_name', 'second_name', 'birthdate', 'biography' and/or 'city'"));
        return false;
    }

    if (!body["password"].is_string()
    ||  !body["first_name"].is_string()
    ||  !body["second_name"].is_string()
    ||  !body["birthdate"].is_string()
    ||  !body["biography"].is_string()
    ||  !body["city"].is_string()) {
        LOGGER_ERROR(std::format("user_register_handler: request params 'password', 'first_name', 'second_name', 'birthdate', 'biography' and 'city' should be a string"));
        return false;
    }

    if (body["password"].get<std::string>().length() < 8) {
        // минимальная длина 8 символов
        LOGGER_ERROR(std::format("user_register_handler: request param 'password' should contain at least 8 characters"));
        return false;
    }

    {
        std::tm t{};
        std::istringstream ss(body["birthdate"].get<std::string>());
        ss >> std::get_time(&t, "%Y-%m-%d"); // 2017-02-01
        if (ss.fail() || t.tm_year < 0 || t.tm_year > 107) {
            // 18лет (с 2007 г.д.), 2007 - 1900 = 107
            LOGGER_ERROR(std::format("user_register_handler: request param 'birthdate' is invalid"));
            return false;
        }
    }

    if (!db_) {
        auto err = std::format("user_register_handler: database service is unavailable");
        LOGGER_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res->setStatusCode(drogon::HttpStatusCode::k503ServiceUnavailable);
        res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        res->setBody(j.dump());
        return false;
    }

    const std::string pwd{body["password"].get<std::string>()};
    const std::string fname{body["first_name"].get<std::string>()};
    const std::string sname{body["second_name"].get<std::string>()};
    const std::string bdate{body["birthdate"].get<std::string>()};
    const std::string bio{body["biography"].get<std::string>()};
    const std::string city{body["city"].get<std::string>()};

    std::string err{};
    try {
        auto rv = db_->register_user(fname, sname, bdate, bio, city, pwd);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (!rv.user.has_value()) {
                err = std::format("can't register user '{} {}'", fname, sname);
            } else {
                // успешная регистрация пользователя
                nlohmann::json j = {{"user_id", rv.user.value().id}};
                res->setStatusCode(drogon::HttpStatusCode::k200OK);
                res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                res->setBody(j.dump());
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("user_register_handler exception: {} (fname: {}, sname: {}, bdate: {}, city: {}, pwd: {})", ex.what(), fname, sname, bdate, city, std::string(pwd.size(), '*'));
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

bool HttpUserService::user_get_id_handler(const drogon::HttpRequestPtr& /*req*/, drogon::HttpResponsePtr& res, const std::string& requested_id)
{
    if (!AuthService::is_valid_uuid(requested_id)) {
        LOGGER_ERROR(std::format("user_get_id_handler: request param 'id' is not an UUID format"));
        return false;
    }

    if (!db_) {
        auto err = std::format("user_get_id_handler: database service is unavailable");
        LOGGER_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res->setStatusCode(drogon::HttpStatusCode::k503ServiceUnavailable);
        res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        res->setBody(j.dump());
        return false;
    }

    std::string err{};
    try {
        auto rv = db_->get_user(requested_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            if (!rv.user.has_value()) {
                // анкета не найдена
                res->setStatusCode(drogon::HttpStatusCode::k404NotFound);
                return false;
            } else {
                // успешное получение анкеты пользователя
                res->setStatusCode(drogon::HttpStatusCode::k200OK);
                res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
                res->setBody(serialize_user(rv.user.value()));
                return true;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("user_get_id_handler exception: {} (user_id: {})", ex.what(), requested_id);
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

bool HttpUserService::user_search_handler(const drogon::HttpRequestPtr& req, drogon::HttpResponsePtr& res)
{
    std::string req_fname;
    std::string req_sname;

    {
        auto v = req->getOptionalParameter<std::string>("first_name");
        if (!v.has_value()) {
            LOGGER_ERROR(std::format("user_search_handler: request params does not contain 'first_name'"));
            return false;
        }
        req_fname = v.value();
    }
    {
        auto v = req->getOptionalParameter<std::string>("last_name");
        if (!v.has_value()) {
            LOGGER_ERROR(std::format("user_search_handler: request params does not contain 'last_name'"));
            return false;
        }
        req_sname = v.value();
    }

    if (!db_) {
        auto err = std::format("user_search_handler: database service is unavailable");
        LOGGER_ERROR(err);
        nlohmann::json j = {{"code", 503}, {"message", err}};
        res->setStatusCode(drogon::HttpStatusCode::k503ServiceUnavailable);
        res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        res->setBody(j.dump());
        return false;
    }

    const std::string fname{req_fname + "%"};
    const std::string sname{req_sname + "%"};

    std::string err{};
    try {
        auto rv = db_->search_user(fname, sname);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            res->setStatusCode(drogon::HttpStatusCode::k200OK);
            res->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
            res->setBody(serialize_users(rv.users));
            return true;
        }
    } catch (std::exception& ex) {
        err = std::format("user_search_handler exception: {} (fname: '{}', sname: '{}')", ex.what(), fname, sname);
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

std::string HttpUserService::serialize_users(const std::vector<DatabaseService::User>& users)
{
    nlohmann::json j{};
    if (users.empty()) {
        j = nlohmann::json::array({});
    } else {
        for (const auto& u : users) {
            j.push_back({{"id",          u.id},
                         {"first_name",  u.first_name},
                         {"second_name", u.second_name},
                         {"birthdate",   u.birthdate},
                         {"biography",   u.biography},
                         {"city",        u.city}});
        }
    }
    return j.dump();
}

std::string HttpUserService::serialize_user(const DatabaseService::User& user)
{
    nlohmann::json j = {{"id",          user.id},
                        {"first_name",  user.first_name},
                        {"second_name", user.second_name},
                        {"birthdate",   user.birthdate},
                        {"biography",   user.biography},
                        {"city",        user.city}};
    return j.dump();
}
