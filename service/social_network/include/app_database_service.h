#pragma once

#include <optional>
#include <vector>
#include "app_metrics.h"
#include "configuration.h"

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
        uint64_t    created_at_msec{};
    };
    struct Message {
        std::string from{};
        std::string to{};
        std::string text{};
        uint64_t    created_at_msec{};
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
    struct post_rv {
        std::string error_str{};
        std::optional<Post> post{};
    };
    struct posts_rv {
        std::string error_str{};
        std::vector<Post> posts{};
    };
    struct dialog_rv {
        std::string error_str{};
        std::vector<Message> messages{};
    };

public:
    ~DatabaseService() = default;
    DatabaseService() = delete;
    DatabaseService(const DatabaseService&) = delete;
    DatabaseService(DatabaseService&&) = delete;
    DatabaseService& operator=(const DatabaseService&) = delete;
    DatabaseService& operator=(DatabaseService&&) = delete;

    explicit DatabaseService(std::shared_ptr<Metrics> metrics)
    :   logger_(Configuration::instance().get_logger()),
        metrics_(std::move(metrics)) {}

    auth_rv authenticate_user(const std::string& user_id);
    login_rv login_user(const std::string& user_id, const std::string& user_pwd);
    user_rv register_user(const std::string& fname, const std::string& sname, const std::string& bdate, const std::string& bio, const std::string& city, const std::string& pwd);
    user_rv get_user(const std::string& user_id);
    users_rv search_user(const std::string& fname, const std::string& sname);

    common_rv add_friend(const std::string& user_id, const std::string& friend_id);
    common_rv delete_friend(const std::string& user_id, const std::string& friend_id);
    friends_rv get_friends(const std::string& user_id);

    post_rv create_post(const std::string& content, const std::string& user_id);
    common_rv update_post(const std::string& post_id, const std::string& content, const std::string& user_id);
    common_rv delete_post(const std::string& post_id, const std::string& user_id);
    post_rv get_post(const std::string& post_id);
    posts_rv feed_post(const std::string& user_id, uint32_t limit);

    common_rv send_dialog_message(const std::string& from_id, const std::string& to_id, const std::string& message);
    dialog_rv list_dialog_messages(const std::string& from_id, const std::string& to_id, uint32_t limit);

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<Metrics>         metrics_{nullptr};

    static std::string calculate_dialog_shard_key(const std::string& from_id, const std::string& to_id);
};
