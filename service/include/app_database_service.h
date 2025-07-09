#pragma once

#include <optional>
#include <vector>
#include "logger/logger.h"
#include "app_metrics.h"
#include "app_connection_pool.h"

namespace SocialNetwork {

class DatabaseService
{
public:
    struct User {
        std::string id{};
        std::string first_name{};
        std::string second_name{};
        std::string birthdate{};
        std::string biography{};
        std::string city{};
    };
    struct Post {
        std::string id{};
        std::string author_user_id{};
        std::string text{};
    };

    struct common_rv {
        std::string error_str{};
    };
    struct auth_rv {
        std::string error_str{};
        bool        authenticated{false};
    };
    struct login_rv {
        std::string error_str{};
        std::string token{};
    };
    struct reguser_rv {
        std::string error_str{};
        std::string user_id{};
    };
    struct user_rv {
        std::string error_str{};
        std::optional<User> user{};
    };
    struct users_rv {
        std::string error_str{};
        std::vector<User> users{};
    };
    struct friends_rv {
        std::string error_str{};
        std::vector<std::string> friend_ids{};
    };
    struct regpost_rv {
        std::string error_str{};
        std::string post_id{};
    };
    struct post_rv {
        std::string error_str{};
        std::optional<Post> post{};
    };
    struct posts_rv {
        std::string error_str{};
        std::vector<Post> posts{};
    };

public:
    ~DatabaseService() = default;
    DatabaseService() = delete;
    DatabaseService(const DatabaseService&) = delete;
    DatabaseService(DatabaseService&&) = delete;
    DatabaseService& operator=(const DatabaseService&) = delete;
    DatabaseService& operator=(DatabaseService&&) = delete;

    explicit DatabaseService(std::shared_ptr<Logging::Logger> logger,
                             std::shared_ptr<Metrics> metrics,
                             std::shared_ptr<ConnectionPool> db_pool)
    :   logger_(std::move(logger)),
        metrics_(std::move(metrics)),
        db_pool_(std::move(db_pool)) {}

    auth_rv authenticate_user(const std::string& user_id);
    login_rv login_user(const std::string& user_id, const std::string& user_pwd);
    reguser_rv register_user(const std::string& fname, const std::string& sname, const std::string& bdate, const std::string& bio, const std::string& city, const std::string& pwd);
    user_rv get_user(const std::string& user_id);
    users_rv search_user(const std::string& fname, const std::string& sname);

    common_rv add_friend(const std::string& user_id, const std::string& friend_id);
    common_rv delete_friend(const std::string& user_id, const std::string& friend_id);
    friends_rv get_friends(const std::string& user_id);

    regpost_rv create_post(const std::string& content, const std::string& user_id);
    common_rv update_post(const std::string& post_id, const std::string& content, const std::string& user_id);
    common_rv delete_post(const std::string& post_id, const std::string& user_id);
    post_rv get_post(const std::string& post_id);
    posts_rv feed_post(const std::string& user_id);

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Metrics>         metrics_{nullptr};
    std::shared_ptr<ConnectionPool>  db_pool_{nullptr};
};

} // namespace SocialNetwork
