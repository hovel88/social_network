#pragma once

#include <drogon/drogon.h>
#include "app_auth_service.h"
#include "helpers/string.h"

class HttpMiddlewareAuth : public drogon::HttpFilter<HttpMiddlewareAuth, false>
{
private:
    std::shared_ptr<AuthService> auth_{nullptr};

public:
    ~HttpMiddlewareAuth() = default;
    HttpMiddlewareAuth(std::shared_ptr<AuthService> auth)
    :   auth_(std::move(auth)) {}

    static std::shared_ptr<HttpMiddlewareAuth> create(std::shared_ptr<AuthService> auth) {
        return std::make_shared<HttpMiddlewareAuth>(auth);
    }

    virtual void doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) override {
        if (auth_) {
            auto auth_header = req->getHeader("Authorization");
            if (!auth_header.empty()
            &&  auth_header.find("Bearer ") == 0) {
                std::string user_id = StringHelpers::to_lowercase(auth_header.substr(7));
                if (AuthService::is_valid_uuid(user_id)) {
                    if (auth_->authenticate(user_id)) {
                        // сохраним для последующего использования
                        req->attributes()->insert("user_id", user_id);
                        // продолжаем цепочку выполнения
                        return fccb();
                    }
                }
            }
        }
        auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k401Unauthorized, drogon::ContentType::CT_APPLICATION_JSON);
        res->setBody("{\"error\":\"Unauthorized\"}");
        return fcb(res);
    }
};
