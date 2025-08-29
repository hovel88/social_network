#pragma once

#include <tuple>
#include <set>
#include <utility>
#include <queue>
#include <mutex>
#include <memory>
#include <format>
#include <stdexcept>
#include <pqxx/pqxx>
#include "logger/logger.h"
#include "helpers/url.h"
#include "configuration.h"

class ConnectionPool
{
public:
    enum class NodeType { MASTER, REPLICA };

    using TypedConnection = std::tuple<NodeType, std::shared_ptr<pqxx::connection>>; // <node_type, conn>

public:
    static ConnectionPool& instance()
    {
        static ConnectionPool instance;
        return instance;
    }

    TypedConnection get_master_connection()
    {
        if (master_conn_str_.empty()) {
            throw std::runtime_error("no available master connections");
        }
        std::lock_guard<std::mutex> lock(master_mtx_);

        // сначала поищем подходящее соединение в пуле
        while (!master_connections_.empty()) {
            auto conn = std::move(master_connections_.front());
            if (conn->is_open()) {
                // соединение, которое до сих пор активно. используем его
                master_connections_.pop();
                return std::make_tuple(NodeType::MASTER, std::move(conn));
            } else {
                // в пуле соединение есть, но оно уже не активно. подчистим
                master_connections_.pop();
            }
        }
        // если мы здесь, значит открытых соединений в пуле не нашлось
        try {
            auto conn = std::make_shared<pqxx::connection>(master_conn_str_);
            return std::make_tuple(NodeType::MASTER, std::move(conn));
        } catch (std::exception& ex) {
            throw std::runtime_error(std::format("failed to create master connection (tag='{}'): {}", master_conn_tag_, ex.what()));
        }
    }

    TypedConnection get_replica_connection()
    {
        if (replica_conn_str_.empty()
        &&  master_conn_str_.empty()) {
            throw std::runtime_error("no available replica connections");
        }
        if (!replica_conn_str_.empty()) {
            std::lock_guard<std::mutex> lock(replica_mtx_);

            // сначала поищем подходящее соединение в пуле
            while (!replica_connections_.empty()) {
                auto conn = std::move(replica_connections_.front());
                if (conn->is_open()) {
                    // соединение, которое до сих пор активно. используем его
                    replica_connections_.pop();
                    return std::make_tuple(NodeType::REPLICA, std::move(conn));
                } else {
                    // в пуле соединение есть, но оно уже не активно. подчистим
                    replica_connections_.pop();
                }
            }
            // если мы здесь, значит открытых соединений в пуле не нашлось
            try {
                auto conn = std::make_shared<pqxx::connection>(replica_conn_str_);
                return std::make_tuple(NodeType::REPLICA, std::move(conn));
            } catch (std::exception& ex) {
                throw std::runtime_error(std::format("failed to create replica connection (tag='{}'): {}", replica_conn_tag_, ex.what()));
            }
        }
        // предпочитаемое соединение - соединение с репликой,
        // но в настройках не было задано URL до реплики,
        // поэтому попытаемся взять соединение до мастера
        return get_master_connection();
    }

    void release_connection(const NodeType& node_type, std::shared_ptr<pqxx::connection> conn)
    {
        if (node_type == NodeType::MASTER) {
            std::lock_guard<std::mutex> lock(master_mtx_);
            master_connections_.push(std::move(conn));
            while (master_connections_.size() > max_connections_per_type_) {
                master_connections_.pop();
            }
        }
        if (node_type == NodeType::REPLICA) {
            std::lock_guard<std::mutex> lock(replica_mtx_);
            replica_connections_.push(std::move(conn));
            while (replica_connections_.size() > max_connections_per_type_) {
                replica_connections_.pop();
            }
        }
    }

    std::set<std::string> get_host_tags() const
    {
        std::set<std::string> tags;
        tags.insert(master_conn_tag_);
        tags.insert(replica_conn_tag_);
        return tags;
    }

private:
    ~ConnectionPool()
    {
        while (!master_connections_.empty()) {
            master_connections_.pop();
        }
        while (!replica_connections_.empty()) {
            replica_connections_.pop();
        }
    }
    ConnectionPool()
    {
        const Configuration& configuration = Configuration::instance();
        max_connections_per_type_ = configuration.http_threads_count;
        {
            UrlHelpers::Url url(configuration.pgsql_master.url);
            master_conn_tag_ = std::format("{}:{}", url.get_host(), url.get_port());
            master_conn_str_ = std::format("user={} password={} host={} port={} dbname={} application_name=social_network connect_timeout=60 keepalives=1 keepalives_idle=60 keepalives_interval=10 keepalives_count=10",
                configuration.pgsql_master.login,
                configuration.pgsql_master.password,
                url.get_host(),
                url.get_port(),
                url.get_path().substr(1));
        }
        {
            UrlHelpers::Url url(configuration.pgsql_replica.url);
            replica_conn_tag_ = std::format("{}:{}", url.get_host(), url.get_port());
            replica_conn_str_ = std::format("user={} password={} host={} port={} dbname={} application_name=social_network connect_timeout=60 keepalives=1 keepalives_idle=60 keepalives_interval=10 keepalives_count=10",
                configuration.pgsql_replica.login,
                configuration.pgsql_replica.password,
                url.get_host(),
                url.get_port(),
                url.get_path().substr(1));
        }
    }

private:
    size_t max_connections_per_type_{0};

    std::string master_conn_tag_{};
    std::string replica_conn_tag_{};

    std::string master_conn_str_{};
    std::string replica_conn_str_{};

    std::mutex                                    master_mtx_{};
    std::queue<std::shared_ptr<pqxx::connection>> master_connections_{};

    std::mutex                                    replica_mtx_{};
    std::queue<std::shared_ptr<pqxx::connection>> replica_connections_{};
};

class ScopedConnection
{
public:
    ~ScopedConnection()
    {
        auto& pool = ConnectionPool::instance();
        pool.release_connection(node_type_, std::move(conn_));
    }
    ScopedConnection(ConnectionPool::NodeType t = ConnectionPool::NodeType::REPLICA)
    {
        auto& pool = ConnectionPool::instance();
        try {
            auto tc    = (t == ConnectionPool::NodeType::MASTER) ? pool.get_master_connection() : pool.get_replica_connection();
            node_type_ = std::get<0>(tc);
            conn_      = std::move(std::get<1>(tc));
        } catch (std::exception& ex) {
            throw std::runtime_error(std::format("failed to get connection: {}", std::string(ex.what())));
        }
    }
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection(ScopedConnection&&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    ScopedConnection& operator=(ScopedConnection&&) = delete;

    pqxx::connection& get()
    {
        if (conn_) return *conn_;
        throw std::runtime_error("connection is not initialized");
    }
    std::string node_tag()
    {
        if (conn_) return std::format("{}:{}", conn_->hostname(), conn_->port());
        return std::string("<?>");
    }
    std::string to_string()
    {
        if (conn_) return std::format("query to {} tag='{}'", (node_type_ == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), node_tag());
        return std::string("<?>");
    }

private:
    std::shared_ptr<pqxx::connection> conn_{nullptr};
    ConnectionPool::NodeType          node_type_{ConnectionPool::NodeType::MASTER};
};
