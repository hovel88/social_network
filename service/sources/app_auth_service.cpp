#include <format>
#include "app_auth_service.h"
#include "helpers/string.h"

bool AuthService::authenticate(std::string& user_id)
{
    std::string err{};
    if (inmem_) {
        auto rv = inmem_->authenticate_user(user_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            return rv.authenticated;
        }

        if (!err.empty()) {
            LOGGER_ERROR(err);
        }
    }
    // if (db_) {
    //     std::string err{};
    //     auto rv = db_->authenticate_user(user_id);
    //     if (!rv.error_str.empty()) {
    //         err = rv.error_str;
    //     } else {
    //         return rv.authenticated;
    //     }

    //     if (!err.empty()) {
    //         LOGGER_ERROR(err);
    //     }
    // }

    return false;
}
