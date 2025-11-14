#include "http_likes_controller.h"

#include <nlohmann/json.hpp>
#include <format>

#include "auth.h"
#include "configuration.h"
#include "logger/logger.h"
#include "helpers/uuid.h"
#include "helpers/url.h"

void HttpLikesController::initPathRouting()
{
    registerMethodViaRegex(&HttpLikesController::add_like,          R"(/likes/([0-9a-fA-F-]{36}))",         {drogon::HttpMethod::Post,   "HttpAuthMiddleware"});
    registerMethodViaRegex(&HttpLikesController::remove_like,       R"(/likes/([0-9a-fA-F-]{36}))",         {drogon::HttpMethod::Delete, "HttpAuthMiddleware"});
    registerMethodViaRegex(&HttpLikesController::get_likes_count,   R"(/likes/([0-9a-fA-F-]{36}))",         {drogon::HttpMethod::Get,    "HttpAuthMiddleware"});
    registerMethodViaRegex(&HttpLikesController::get_likes_details, R"(/likes/details/([0-9a-fA-F-]{36}))", {drogon::HttpMethod::Get,    "HttpAuthMiddleware"});
}

//
// POST /likes/5bbb0d11-b052-4c43-b3c5-85694d27f13a
// Authorization: Bearer bd498662-1313-4aff-ae1a-2b26e227875b
//
// 200 OK
// {
//   "saga_id": "like_saga_550e8400-e29b-41d4-a716-446655440000",
//   "total_likes": 42,
//   "message": "",
//   "success": true
// }
//
void HttpLikesController::add_like(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& post_id)
{
    LOGGER_DEBUG("handler: POST /likes/:post_id");
    const std::string user_id = req->attributes()->get<std::string>("user_id");
    auto resp = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

    if (!Auth::is_valid_uuid(post_id)) {
        LOGGER_ERROR(std::format("add_like: request param 'post_id' is not an UUID format"));
        callback(resp);
        return;
    }

    std::string err{};
    try {
        auto saga_id    = saga_.add_like(user_id, post_id);
        auto status     = saga_.get_status(saga_id);
        auto status_msg = saga_.get_status_message(saga_id);
        auto total      = saga_.get_result(saga_id);

        nlohmann::json j{};
        j = {
            {"saga_id",     saga_id},
            {"total_likes", total},
            {"message",     status_msg},
            {"success",     (status == LikesSaga::Status::COMPLETED)}
            };

        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        resp->setBody(j.dump());
    } catch (std::exception& ex) {
        err = std::format("add_like: failed to add like: {}", ex.what());
        LOGGER_ERROR(err);
    }

    if (!err.empty()) {
        nlohmann::json j = {{"code", 500}, {"message", err}};
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        resp->setBody(j.dump());
    }
    callback(resp);
}

//
// DELETE /likes/5bbb0d11-b052-4c43-b3c5-85694d27f13a
// Authorization: Bearer bd498662-1313-4aff-ae1a-2b26e227875b
//
// 200 OK
// {
//   "saga_id": "like_saga_550e8400-e29b-41d4-a716-446655440000",
//   "total_likes": 41,
//   "message": "",
//   "success": true
// }
//
void HttpLikesController::remove_like(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& post_id)
{
    LOGGER_DEBUG("handler: DELETE /likes/:post_id");
    const std::string user_id = req->attributes()->get<std::string>("user_id");
    auto resp = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

    if (!Auth::is_valid_uuid(post_id)) {
        LOGGER_ERROR(std::format("remove_like: request param 'post_id' is not an UUID format"));
        callback(resp);
        return;
    }

    std::string err{};
    try {
        auto saga_id    = saga_.del_like(user_id, post_id);
        auto status     = saga_.get_status(saga_id);
        auto status_msg = saga_.get_status_message(saga_id);
        auto total      = saga_.get_result(saga_id);

        nlohmann::json j{};
        j = {
            {"saga_id",     saga_id},
            {"total_likes", total},
            {"message",     status_msg},
            {"success",     (status == LikesSaga::Status::COMPLETED)}
            };

        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        resp->setBody(j.dump());

    } catch (const std::exception& e) {
        err = std::format("remove_like: failed to remove like: {}", e.what());
        LOGGER_ERROR(err);
    }

    if (!err.empty()) {
        nlohmann::json j = {{"code", 500}, {"message", err}};
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        resp->setBody(j.dump());
    }
    callback(resp);
}

//
// GET /likes/5bbb0d11-b052-4c43-b3c5-85694d27f13a
// Authorization: Bearer bd498662-1313-4aff-ae1a-2b26e227875b
//
// 200 OK
// {
//   "total_likes": 40
// }
//
void HttpLikesController::get_likes_count(const drogon::HttpRequestPtr& /*req*/, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& post_id)
{
    LOGGER_DEBUG("handler: GET /likes/:post_id");
    // const std::string user_id = req->attributes()->get<std::string>("user_id");
    auto resp = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

    if (!Auth::is_valid_uuid(post_id)) {
        LOGGER_ERROR(std::format("get_likes_count: request param 'post_id' is not an UUID format"));
        callback(resp);
        return;
    }

    std::string err{};
    try {
        auto rv = grpc_client_.get_likes_amount(post_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            nlohmann::json j = {{"total_likes", rv.likes_amount}};
            resp->setStatusCode(drogon::HttpStatusCode::k200OK);
            resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
            resp->setBody(j.dump());
        }
    } catch (const std::exception& e) {
        err = std::format("get_likes_count: failed to get amount of likes: {}", e.what());
        LOGGER_ERROR(err);
    }

    if (!err.empty()) {
        nlohmann::json j = {{"code", 500}, {"message", err}};
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        resp->setBody(j.dump());
    }
    callback(resp);
}

//
// GET /likes/details/5bbb0d11-b052-4c43-b3c5-85694d27f13a?limit=10&offset=0
// Authorization: Bearer bd498662-1313-4aff-ae1a-2b26e227875b
//
// 200 OK
// [
//   {
//     "id": 1234567,
//     "user_id": "bd498662-1313-4aff-ae1a-2b26e227875b",
//     "post_id": "5bbb0d11-b052-4c43-b3c5-85694d27f13a",
//     "saga_id": "saga_550e8400-e29b-41d4-a716-446655440000",
//     "op_type": "INCREMENT",
//     "created_at": 1654473600100
//   }
// ]
//
void HttpLikesController::get_likes_details(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, const std::string& post_id)
{
    LOGGER_DEBUG("handler: GET /likes/details/:post_id");
    // const std::string user_id = req->attributes()->get<std::string>("user_id");
    auto resp = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k400BadRequest, drogon::ContentType::CT_NONE);

    if (!Auth::is_valid_uuid(post_id)) {
        LOGGER_ERROR(std::format("get_likes_details: request param 'post_id' is not an UUID format"));
        callback(resp);
        return;
    }

    size_t offset = 0;
    size_t limit  = 50;

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

    std::string err{};
    try {
        auto rv = db_.get_likes_for_post(post_id, limit, offset);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            // добавляем в ответ несколько заголовков с информацией о параметрах "текущей страницы"
            resp->addHeader("X-Pagination-Offset", std::to_string(offset));
            resp->addHeader("X-Pagination-Limit", std::to_string(limit));

            nlohmann::json j{};
            if (rv.likes_info.empty()) {
                j = nlohmann::json::array({});
            } else {
                for (const auto& l : rv.likes_info) {
                    j.push_back({
                                {"id",         l.id},
                                {"user_id",    l.user_id},
                                {"post_id",    l.post_id},
                                {"saga_id",    l.saga_id},
                                {"op_type",    l.op_type}
                                });
                }
            }

            resp->setStatusCode(drogon::HttpStatusCode::k200OK);
            resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
            resp->setBody(j.dump());
        }
    } catch (const std::exception& e) {
        err = std::format("get_likes_details: failed to get details of likes: {}", e.what());
        LOGGER_ERROR(err);
    }

    if (!err.empty()) {
        nlohmann::json j = {{"code", 500}, {"message", err}};
        resp->setStatusCode(drogon::HttpStatusCode::k500InternalServerError);
        resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);
        resp->setBody(j.dump());
    }
    callback(resp);
}
