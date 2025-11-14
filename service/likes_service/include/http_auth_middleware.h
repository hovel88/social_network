#pragma once

#include <drogon/drogon.h>

#include "auth.h"
#include "helpers/string.h"

class HttpAuthMiddleware : public drogon::HttpFilter<HttpAuthMiddleware, true>
{
public:
    ~HttpAuthMiddleware() = default;
    HttpAuthMiddleware()
    :   auth_(Auth::instance())
    {}
    HttpAuthMiddleware(const HttpAuthMiddleware&) = default;
    HttpAuthMiddleware(HttpAuthMiddleware&&) = default;
    HttpAuthMiddleware& operator=(const HttpAuthMiddleware&) = default;
    HttpAuthMiddleware& operator=(HttpAuthMiddleware&&) = default;

    virtual void doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) override {
        auto auth_header = req->getHeader("Authorization");
        if (!auth_header.empty()
        &&  auth_header.find("Bearer ") == 0) {
            std::string user_id = StringHelpers::to_lowercase(auth_header.substr(7));
            if (Auth::is_valid_uuid(user_id)) {
                if (auth_.authenticate(user_id)) {
                    // сохраним для последующего использования
                    req->attributes()->insert("user_id", user_id);
                    // продолжаем цепочку выполнения
                    return fccb();
                }
            }
        }

        auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k401Unauthorized, drogon::ContentType::CT_APPLICATION_JSON);
        res->setBody("{\"error\":\"Unauthorized\"}");
        return fcb(res);
    }

private:
    Auth& auth_;
};
