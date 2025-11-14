#pragma once

#include <optional>
#include <vector>

#include "configuration.h"

class Database
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
    struct LikeInfo {
        int64_t     id{};
        std::string user_id{};
        std::string post_id{};
        std::string saga_id{};
        std::string op_type{};
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
    struct likes_rv {
        std::string error_str{};
        std::optional<int32_t> likes{};
    };
    struct likes_info_rv {
        std::string error_str{};
        std::vector<LikeInfo> likes_info{};
    };
    struct dialog_rv {
        std::string error_str{};
        std::vector<Message> messages{};
    };

public:
    ~Database() = default;
    Database(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(const Database&) = delete;
    Database& operator=(Database&&) = delete;

    static Database& instance()
    {
        static Database singleton;
        return singleton;
    }

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
    likes_rv inc_post_likes(const std::string& post_id);
    likes_rv dec_post_likes(const std::string& post_id);
    post_rv get_post(const std::string& post_id);
    posts_rv feed_post(const std::string& user_id, uint32_t limit);

    common_rv update_like(const std::string& user_id, const std::string& post_id, const std::string& saga_id, const std::string& op_type);
    common_rv remove_like_by_saga(const std::string& saga_id);
    likes_info_rv get_likes_for_post(const std::string& post_id, int limit = 50, int offset = 0);

    common_rv send_dialog_message(const std::string& from_id, const std::string& to_id, const std::string& message);
    dialog_rv list_dialog_messages(const std::string& from_id, const std::string& to_id, uint32_t limit);

private:
    Database()
    :   logger_(Configuration::instance().get_logger())
    {}

    static std::string calculate_dialog_shard_key(const std::string& from_id, const std::string& to_id);

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
};
