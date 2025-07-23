#include <format>
#include <bcrypt/BCrypt.hpp>
#include "app_database_service.h"

DatabaseService::auth_rv DatabaseService::authenticate_user(const std::string& user_id)
{
    static const std::string query =
        "SELECT 1 "
        "  FROM users "
        " WHERE id = $1";

    auth_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("authenticate_user: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{user_id});
            tx.commit();

            rv.error_str.clear();
            rv.authenticated = !result.empty();
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
            rv.authenticated = false;
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
        rv.authenticated = false;
    }
    return rv;
}

DatabaseService::login_rv DatabaseService::login_user(const std::string& user_id, const std::string& user_pwd)
{
    static const std::string query =
        "SELECT id, pwd_hash "
        "  FROM users "
        " WHERE id = $1";

    login_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("login_user: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{user_id});
            tx.commit();

            rv.error_str.clear();
            rv.token.clear();

            for (const auto& row : result) {
                const auto& [row_id, row_pwd_hash] = row.as<std::string, std::string>();
                if (!BCrypt::validatePassword(user_pwd, row_pwd_hash)) {
                    rv.error_str = std::format("login_user: request param 'password' is not match");
                } else {
                    rv.token = row_id;
                }
                break;
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::user_rv DatabaseService::register_user(const std::string& fname, const std::string& sname, const std::string& bdate, const std::string& bio, const std::string& city, const std::string& pwd)
{
    static const std::string query =
        "INSERT INTO users (first_name, second_name, birthdate, biography, city, pwd_hash) "
        "     VALUES ($1, $2, $3, $4, $5, $6) "
        "  RETURNING id";

    user_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("register_user: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            const std::string hashed_pwd = BCrypt::generateHash(pwd, 12);

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{fname, sname, bdate, bio, city, hashed_pwd});
            tx.commit();

            rv.error_str.clear();
            rv.user = std::nullopt;

            for (const auto& row : result) {
                const auto& [row_id] = row.as<std::string>();
                rv.user = User{};
                rv.user->id             = row_id;
                rv.user->first_name     = fname;
                rv.user->second_name    = sname;
                rv.user->birthdate      = bdate;
                rv.user->biography      = bio;
                rv.user->city           = city;
                break;
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::user_rv DatabaseService::get_user(const std::string& user_id)
{
    static const std::string query =
        "SELECT first_name, second_name, birthdate, biography, city "
        "  FROM users "
        " WHERE id = $1";

    user_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("get_user: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{user_id});
            tx.commit();

            rv.error_str.clear();
            rv.user = std::nullopt;

            for (const auto& row : result) {
                const auto& [row_fname, row_sname, row_bdate, row_bio, row_city] = row.as<std::string, std::string, std::string, std::string, std::string>();
                rv.user = User{};
                rv.user->id             = user_id;
                rv.user->first_name     = row_fname;
                rv.user->second_name    = row_sname;
                rv.user->birthdate      = row_bdate;
                rv.user->biography      = row_bio;
                rv.user->city           = row_city;
                break;
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::users_rv DatabaseService::search_user(const std::string& fname, const std::string& sname)
{
    static const std::string query =
        "SELECT id, first_name, second_name, birthdate, biography, city "
        "  FROM users "
        " WHERE first_name LIKE $1 AND second_name LIKE $2 "
        " ORDER BY id "
        " LIMIT 250";

    users_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("search_user: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{fname, sname});
            tx.commit();

            rv.error_str.clear();
            rv.users.clear();

            for (const auto& row : result) {
                const auto& [row_id, row_fname, row_sname, row_bdate, row_bio, row_city] = row.as<std::string, std::string, std::string, std::string, std::string, std::string>();
                auto& u = rv.users.emplace_back(User());
                u.id            = row_id;
                u.first_name    = row_fname;
                u.second_name   = row_sname;
                u.birthdate     = row_bdate;
                u.biography     = row_bio;
                u.city          = row_city;
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::common_rv DatabaseService::add_friend(const std::string& user_id, const std::string& friend_id)
{
    static const std::string query =
        "INSERT INTO friends (user_id, friend_id) "
        "VALUES ($1, $2) "
        "ON CONFLICT (user_id, friend_id) DO NOTHING";

    common_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("add_friend: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            // отношения дружбы в обе стороны
            tx.exec(query, pqxx::params{user_id, friend_id});
            tx.exec(query, pqxx::params{friend_id, user_id});
            tx.commit();

            rv.error_str.clear();
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::common_rv DatabaseService::delete_friend(const std::string& user_id, const std::string& friend_id)
{
    static const std::string query =
        "DELETE FROM friends "
        " WHERE user_id = $1 AND friend_id = $2";

    common_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("delete_friend: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            // отношения дружбы в обе стороны
            tx.exec(query, pqxx::params{user_id, friend_id});
            tx.exec(query, pqxx::params{friend_id, user_id});
            tx.commit();

            rv.error_str.clear();
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::friends_rv DatabaseService::get_friends(const std::string& user_id)
{
    static const std::string query =
        "SELECT friend_id "
        "  FROM friends "
        " WHERE user_id = $1";

    friends_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("get_friends: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{user_id});
            tx.commit();

            rv.error_str.clear();
            rv.friend_ids.clear();

            if (!result.empty()) {
                rv.friend_ids.reserve(result.size());
            }
            for (const auto& row : result) {
                const auto& [row_friend_id] = row.as<std::string>();
                rv.friend_ids.push_back(row_friend_id);
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::post_rv DatabaseService::create_post(const std::string& content, const std::string& user_id)
{
    static const std::string query =
        "INSERT INTO posts (user_id, content) VALUES ($1, $2) "
        "RETURNING id, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms";

    post_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("create_post: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{user_id, content});
            tx.commit();

            rv.error_str.clear();
            rv.post = std::nullopt;

            for (const auto& row : result) {
                const auto& [row_id, row_created_at] = row.as<std::string, uint64_t>();
                rv.post = Post{};
                rv.post->id              = row_id;
                rv.post->author_user_id  = user_id;
                rv.post->text            = content;
                rv.post->created_at_msec = row_created_at;
                break;
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::common_rv DatabaseService::update_post(const std::string& post_id, const std::string& content, const std::string& user_id)
{
    static const std::string query =
        "UPDATE posts SET content = $1, updated_at = NOW() "
        " WHERE id = $2 AND user_id = $3 AND deleted_at IS NULL "
        "RETURNING 1";

    common_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("update_post: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{content, post_id, user_id});
            tx.commit();

            rv.error_str.clear();

            if (result.empty()) {
                rv.error_str = std::format("update_post: invalid post id or user is not an author");
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::common_rv DatabaseService::delete_post(const std::string& post_id, const std::string& user_id)
{
    static const std::string query =
        "UPDATE posts SET deleted_at = NOW() "
        " WHERE id = $1 AND user_id = $2 AND deleted_at IS NULL "
        "RETURNING 1";

    common_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("delete_post: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{post_id, user_id});
            tx.commit();

            rv.error_str.clear();

            if (result.empty()) {
                rv.error_str = std::format("delete_post: invalid post id or user is not an author");
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::post_rv DatabaseService::get_post(const std::string& post_id)
{
    static const std::string query =
        "SELECT user_id, content, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms "
        "  FROM posts "
        " WHERE id = $1 AND deleted_at IS NULL";

    post_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("get_post: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{post_id});
            tx.commit();

            rv.error_str.clear();
            rv.post = std::nullopt;

            for (const auto& row : result) {
                const auto& [row_user_id, row_content, row_created_at] = row.as<std::string, std::string, uint64_t>();
                rv.post = Post{};
                rv.post->id              = post_id;
                rv.post->author_user_id  = row_user_id;
                rv.post->text            = row_content;
                rv.post->created_at_msec = row_created_at;
                break;
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::posts_rv DatabaseService::feed_post(const std::string& user_id, uint32_t limit)
{
    static const std::string query =
        "SELECT p.id, p.user_id, p.content, (EXTRACT(EPOCH FROM p.created_at) * 1000)::bigint as created_at_ms "
        "  FROM posts p "
        "  JOIN friends f ON p.user_id = f.friend_id "
        " WHERE f.user_id = $1 AND p.deleted_at IS NULL "
        " ORDER BY p.created_at DESC "
        " LIMIT $2";

    posts_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("feed_post: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{user_id, std::to_string(limit)});
            tx.commit();

            rv.error_str.clear();
            rv.posts.clear();

            for (const auto& row : result) {
                const auto& [row_id, row_user_id, row_content, row_created_at] = row.as<std::string, std::string, std::string, uint64_t>();
                auto& p = rv.posts.emplace_back(Post());
                p.id              = row_id;
                p.author_user_id  = row_user_id;
                p.text            = row_content;
                p.created_at_msec = row_created_at;
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::common_rv DatabaseService::send_dialog_message(const std::string& from_id, const std::string& to_id, const std::string& message)
{
    static const std::string query =
        "INSERT INTO dialogs (from_user_id, to_user_id, message, shard_key) "
        "VALUES ($1, $2, $3, $4)";

    common_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("send_dialog_message: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            tx.exec(query, pqxx::params{from_id, to_id, message, calculate_dialog_shard_key(from_id, to_id)});
            tx.commit();

            rv.error_str.clear();
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

DatabaseService::dialog_rv DatabaseService::list_dialog_messages(const std::string& from_id, const std::string& to_id, uint32_t limit)
{
    static const std::string query =
        "SELECT from_user_id, to_user_id, message, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms "
        "  FROM dialogs "
        " WHERE (shard_key = $1 OR shard_key = $2) "
        "   AND ((from_user_id = $3 AND to_user_id = $4) OR (from_user_id = $4 AND to_user_id = $3)) "
        " ORDER BY created_at DESC "
        " LIMIT $5";

    dialog_rv rv{};
    if (db_pool_) {
        try {
            ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
            metrics_->count_request_to_host(scoped_conn.node_tag);
            LOG_TRACE(std::format("list_dialog_messages: query to {} #{} tag='{}'",
                (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

            pqxx::work tx(*scoped_conn.conn.get());
            pqxx::result result = tx.exec(query, pqxx::params{calculate_dialog_shard_key(from_id, to_id), calculate_dialog_shard_key(to_id, from_id), from_id, to_id, std::to_string(limit)});
            tx.commit();

            rv.error_str.clear();
            rv.messages.clear();

            for (const auto& row : result) {
                const auto& [row_from_id, row_to_id, row_message, row_created_at] = row.as<std::string, std::string, std::string, uint64_t>();
                auto& p = rv.messages.emplace_back(Message());
                p.from            = row_from_id;
                p.to              = row_to_id;
                p.text            = row_message;
                p.created_at_msec = row_created_at;
            }
        } catch (std::exception& ex) {
            rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to DB (query: {})", query);
    }
    return rv;
}

std::string DatabaseService::calculate_dialog_shard_key(const std::string& from_id, const std::string& to_id)
{
    // для учета эффекта Леди Гаги, можно при старте извлекать из статистики БД
    // ID самых активных пользователей, хранить их и при генерации shard_key
    // учитывать активность пользователя, чтобы направить диалог в отдельный шард,
    // и таким образом равномерно распределять нагрузку таких активных пользователей
    // по разным шардам

    // static const std::unordered_set<std::string> active_users = { ... };
    // bool from_active = active_users.count(from_id);
    // bool to_active   = active_users.count(to_id);
    // if (from_active || to_active) {
    //     auto active_id = from_active ? from_id : to_id;
    //     auto normal_id = from_active ? to_id   : from_id;
    //     return std::format("active_{}_{}", (active_id % 32), normal_id);
    // }

    return std::format("{}_{}", from_id, to_id);
}
