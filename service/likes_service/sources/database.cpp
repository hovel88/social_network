#include "database.h"

#include <bcrypt/BCrypt.hpp>

#include "database_connection_pool.h"

Database::auth_rv Database::authenticate_user(const std::string& user_id)
{
    static const std::string query =
        "SELECT 1 "
        "  FROM users "
        " WHERE id = $1";

    auth_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("authenticate_user: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{user_id});
        tx.commit();

        rv.error_str.clear();
        rv.authenticated = !result.empty();
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
        rv.authenticated = false;
    }
    return rv;
}

Database::login_rv Database::login_user(const std::string& user_id, const std::string& user_pwd)
{
    static const std::string query =
        "SELECT id, pwd_hash "
        "  FROM users "
        " WHERE id = $1";

    login_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::REPLICA);
        LOGGER_TRACE(std::format("login_user: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
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
    return rv;
}

Database::user_rv Database::register_user(const std::string& fname, const std::string& sname, const std::string& bdate, const std::string& bio, const std::string& city, const std::string& pwd)
{
    static const std::string query =
        "INSERT INTO users (first_name, second_name, birthdate, biography, city, pwd_hash) "
        "     VALUES ($1, $2, $3, $4, $5, $6) "
        "  RETURNING id";

    user_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("register_user: {}", scoped_conn.to_string()));

        const std::string hashed_pwd = BCrypt::generateHash(pwd, 12);

        pqxx::work tx(scoped_conn.get());
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
    return rv;
}

Database::user_rv Database::get_user(const std::string& user_id)
{
    static const std::string query =
        "SELECT first_name, second_name, birthdate, biography, city "
        "  FROM users "
        " WHERE id = $1";

    user_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::REPLICA);
        LOGGER_TRACE(std::format("get_user: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
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
    return rv;
}

Database::users_rv Database::search_user(const std::string& fname, const std::string& sname)
{
    static const std::string query =
        "SELECT id, first_name, second_name, birthdate, biography, city "
        "  FROM users "
        " WHERE first_name LIKE $1 AND second_name LIKE $2 "
        " ORDER BY id "
        " LIMIT 250";

    users_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::REPLICA);
        LOGGER_TRACE(std::format("search_user: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
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
    return rv;
}

Database::common_rv Database::add_friend(const std::string& user_id, const std::string& friend_id)
{
    static const std::string query =
        "INSERT INTO friends (user_id, friend_id) "
        "VALUES ($1, $2) "
        "ON CONFLICT (user_id, friend_id) DO NOTHING";

    common_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("add_friend: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        // отношения дружбы в обе стороны
        tx.exec(query, pqxx::params{user_id, friend_id});
        tx.exec(query, pqxx::params{friend_id, user_id});
        tx.commit();

        rv.error_str.clear();
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::common_rv Database::delete_friend(const std::string& user_id, const std::string& friend_id)
{
    static const std::string query =
        "DELETE FROM friends "
        " WHERE user_id = $1 AND friend_id = $2";

    common_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("delete_friend: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        // отношения дружбы в обе стороны
        tx.exec(query, pqxx::params{user_id, friend_id});
        tx.exec(query, pqxx::params{friend_id, user_id});
        tx.commit();

        rv.error_str.clear();
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::friends_rv Database::get_friends(const std::string& user_id)
{
    static const std::string query =
        "SELECT friend_id "
        "  FROM friends "
        " WHERE user_id = $1";

    friends_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::REPLICA);
        LOGGER_TRACE(std::format("get_friends: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
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
    return rv;
}

Database::post_rv Database::create_post(const std::string& content, const std::string& user_id)
{
    static const std::string query =
        "INSERT INTO posts (user_id, content) VALUES ($1, $2) "
        "RETURNING id, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms";

    post_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("create_post: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
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
    return rv;
}

Database::common_rv Database::update_post(const std::string& post_id, const std::string& content, const std::string& user_id)
{
    static const std::string query =
        "UPDATE posts SET content = $1, updated_at = NOW() "
        " WHERE id = $2 AND user_id = $3 AND deleted_at IS NULL "
        "RETURNING 1";

    common_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("update_post: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{content, post_id, user_id});
        tx.commit();

        rv.error_str.clear();

        if (result.empty()) {
            rv.error_str = std::format("update_post: invalid post id or user is not an author");
        }
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::common_rv Database::delete_post(const std::string& post_id, const std::string& user_id)
{
    static const std::string query =
        "UPDATE posts SET deleted_at = NOW() "
        " WHERE id = $1 AND user_id = $2 AND deleted_at IS NULL "
        "RETURNING 1";

    common_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("delete_post: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{post_id, user_id});
        tx.commit();

        rv.error_str.clear();

        if (result.empty()) {
            rv.error_str = std::format("delete_post: invalid post id or user is not an author");
        }
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::likes_rv Database::inc_post_likes(const std::string& post_id)
{
    static const std::string query =
        "UPDATE posts "
        "   SET likes_count = likes_count + 1 "
        " WHERE post_id = $1 "
        "RETURNING likes_count";

    likes_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("inc_post_likes: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{post_id});
        tx.commit();

        rv.error_str.clear();
        rv.likes = std::nullopt;

        for (const auto& row : result) {
            const auto& [row_likes] = row.as<int32_t>();
            *rv.likes = row_likes;
            break;
        }
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::likes_rv Database::dec_post_likes(const std::string& post_id)
{
    static const std::string query =
        "UPDATE posts "
        "   SET likes_count = GREATEST(likes_count - 1, 0) "
        " WHERE post_id = $1 "
        "RETURNING likes_count";

    likes_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("dec_post_likes: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{post_id});
        tx.commit();

        rv.error_str.clear();
        rv.likes = std::nullopt;

        for (const auto& row : result) {
            const auto& [row_likes] = row.as<int32_t>();
            *rv.likes = row_likes;
            break;
        }
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::post_rv Database::get_post(const std::string& post_id)
{
    static const std::string query =
        "SELECT user_id, content, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms "
        "  FROM posts "
        " WHERE id = $1 AND deleted_at IS NULL";

    post_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::REPLICA);
        LOGGER_TRACE(std::format("get_post: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
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
    return rv;
}

Database::posts_rv Database::feed_post(const std::string& user_id, uint32_t limit)
{
    static const std::string query =
        "SELECT p.id, p.user_id, p.content, (EXTRACT(EPOCH FROM p.created_at) * 1000)::bigint as created_at_ms "
        "  FROM posts p "
        "  JOIN friends f ON p.user_id = f.friend_id "
        " WHERE f.user_id = $1 AND p.deleted_at IS NULL "
        " ORDER BY p.created_at DESC "
        " LIMIT $2";

    posts_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::REPLICA);
        LOGGER_TRACE(std::format("feed_post: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
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
    return rv;
}

Database::common_rv Database::update_like(const std::string& user_id, const std::string& post_id, const std::string& saga_id, const std::string& op_type)
{
    static const std::string query =
        "INSERT INTO post_likes (user_id, post_id, saga_id, op_type) "
        "VALUES ($1, $2, $3, $4)";

    common_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("update_like: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        tx.exec(query, pqxx::params{user_id, post_id, saga_id, op_type});
        tx.commit();

        rv.error_str.clear();
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::common_rv Database::remove_like_by_saga(const std::string& saga_id)
{
    static const std::string query =
        "DELETE FROM post_likes "
        " WHERE saga_id = $1";

    common_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("remove_like_by_saga: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        tx.exec(query, pqxx::params{saga_id});
        tx.commit();

        rv.error_str.clear();
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::likes_info_rv Database::get_likes_for_post(const std::string& post_id, int limit, int offset)
{
    static const std::string query =
        "SELECT id, user_id, post_id, saga_id, op_type, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms "
        "  FROM post_likes "
        " WHERE post_id = $1 "
        " ORDER BY created_at DESC "
        " LIMIT $2 OFFSET $3";

    likes_info_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("get_likes_for_post: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{post_id, std::to_string(limit), std::to_string(offset)});
        tx.commit();

        for (const auto& row : result) {
            const auto& [row_id, row_user_id, row_post_id, row_saga_id, row_op_type, row_created_at] = row.as<int64_t, std::string, std::string, std::string, std::string, uint64_t>();
            auto& i = rv.likes_info.emplace_back(LikeInfo());
            i.id              = row_id;
            i.user_id         = row_user_id;
            i.post_id         = row_post_id;
            i.saga_id         = row_saga_id;
            i.op_type         = row_op_type;
            i.created_at_msec = row_created_at;
        }
        rv.error_str.clear();
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::common_rv Database::send_dialog_message(const std::string& from_id, const std::string& to_id, const std::string& message)
{
    static const std::string query =
        "INSERT INTO dialogs (from_user_id, to_user_id, message, shard_key) "
        "VALUES ($1, $2, $3, $4)";

    common_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::MASTER);
        LOGGER_TRACE(std::format("send_dialog_message: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
        tx.exec(query, pqxx::params{from_id, to_id, message, calculate_dialog_shard_key(from_id, to_id)});
        tx.commit();

        rv.error_str.clear();
    } catch (std::exception& ex) {
        rv.error_str = std::format("SQL exception: {} (query: {})", ex.what(), query);
    }
    return rv;
}

Database::dialog_rv Database::list_dialog_messages(const std::string& from_id, const std::string& to_id, uint32_t limit)
{
    static const std::string query =
        "SELECT from_user_id, to_user_id, message, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms "
        "  FROM dialogs "
        " WHERE (shard_key = $1 OR shard_key = $2) "
        "   AND ((from_user_id = $3 AND to_user_id = $4) OR (from_user_id = $4 AND to_user_id = $3)) "
        " ORDER BY created_at DESC "
        " LIMIT $5";

    dialog_rv rv{};
    try {
        ScopedConnection scoped_conn(ConnectionPool::NodeType::REPLICA);
        LOGGER_TRACE(std::format("list_dialog_messages: {}", scoped_conn.to_string()));

        pqxx::work tx(scoped_conn.get());
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
    return rv;
}

std::string Database::calculate_dialog_shard_key(const std::string& from_id, const std::string& to_id)
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
