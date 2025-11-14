#include "auth.h"

bool Auth::authenticate(std::string& user_id)
{
    std::string err{};
    auto rv = db_.authenticate_user(user_id);
    if (!rv.error_str.empty()) {
        err = rv.error_str;
    } else {
        return rv.authenticated;
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
    }

    return false;
}
