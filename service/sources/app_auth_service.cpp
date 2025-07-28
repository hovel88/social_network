#include <format>
#include "app_auth_service.h"
#include "helpers/string.h"

bool AuthService::authenticate(const drogon::HttpRequestPtr& req, std::string& user_id)
{
    auto auth_header = req->getHeader("Authorization");
    if (auth_header.empty()
    ||  auth_header.find("Bearer ") != 0) {
        return false;
    }

    user_id = StringHelpers::to_lowercase(auth_header.substr(7));
    if (!AuthService::is_valid_uuid(user_id)) {
        return false;
    }

    if (!db_) {
        return false;
    }

    std::string err{};
    try {
        auto rv = db_->authenticate_user(user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            return rv.authenticated;
        }
    } catch (std::exception& ex) {
        err = std::format("authenticate exception: {} (user_id: {})", ex.what(), user_id);
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
    }
    return false;
}
