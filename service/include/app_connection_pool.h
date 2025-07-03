#pragma once

#include <format>
#include <tuple>
#include <utility>
#include <queue>
#include <set>
#include <mutex>
#include <memory>
#include <vector>
#include <stdexcept>
#include <pqxx/pqxx>
#include "logger/logger.h"

namespace SocialNetwork {

class ConnectionPool
{
public:
    enum class NodeType { MASTER, REPLICA };

    using TypeNumTagConnection    = std::tuple<NodeType, size_t, std::string, std::shared_ptr<pqxx::connection>>; // <node_type, node_num, node_tag, conn>
    using ConnectionStrCollection = std::vector<std::pair<std::string, std::string>>; // vector of <conn_str, node_tag>

private:
    struct node_s {
        std::string                                   node_tag{};
        std::string                                   conn_str{};
        std::queue<std::shared_ptr<pqxx::connection>> conn_pool{};
    };

    std::mutex                              pool_mtx_{};
    std::map<NodeType, std::vector<node_s>> pool_{}; // [node_type : vector of <node_s> ]

public:
    ConnectionPool(const ConnectionStrCollection& masters,
                   const ConnectionStrCollection& replicas,
                   size_t pool_size) {
        pool_[NodeType::MASTER].clear();
        pool_[NodeType::REPLICA].clear();

        // соединение к master-node
        for (const auto& master : masters) {
            auto& entry = pool_[NodeType::MASTER].emplace_back();
            entry.node_tag = master.second;
            entry.conn_str = master.first;
            for (size_t i = 0; i < pool_size; ++i) {
                entry.conn_pool.emplace(std::make_shared<pqxx::connection>(entry.conn_str));
            }
        }

        // соединения к replica-node
        for (const auto& replica : replicas) {
            auto& entry = pool_[NodeType::REPLICA].emplace_back();
            entry.node_tag = replica.second;
            entry.conn_str = replica.first;
            for (size_t i = 0; i < pool_size; ++i) {
                entry.conn_pool.emplace(std::make_shared<pqxx::connection>(entry.conn_str));
            }
        }
    }

    TypeNumTagConnection get_connection(NodeType preferred) {
        std::lock_guard<std::mutex> lock(pool_mtx_);

        if (!pool_[NodeType::REPLICA].empty()
        &&  preferred == NodeType::REPLICA) {
            // Round Robin
            static size_t last_used_replica_num = 0;
            last_used_replica_num = (last_used_replica_num + 1) % pool_[NodeType::REPLICA].size();

            auto& entry = pool_[NodeType::REPLICA][last_used_replica_num];
            if (!entry.conn_pool.empty()) {
                auto conn = std::move(entry.conn_pool.front());
                entry.conn_pool.pop();
                return std::make_tuple(NodeType::REPLICA, last_used_replica_num, entry.node_tag, std::move(conn));
            }

            throw std::runtime_error("No connections available");
        }

        if (!pool_[NodeType::MASTER].empty()) {
            // Round Robin
            static size_t last_used_master_num = 0;
            last_used_master_num = (last_used_master_num + 1) % pool_[NodeType::MASTER].size();

            auto& entry = pool_[NodeType::MASTER][last_used_master_num];
            if (!entry.conn_pool.empty()) {
                auto conn = std::move(entry.conn_pool.front());
                entry.conn_pool.pop();
                return std::make_tuple(NodeType::MASTER, last_used_master_num, entry.node_tag, std::move(conn));
            }

            throw std::runtime_error("No connections available");
        }

        // случай, когда у нас вообще ничего не настроено
        throw std::runtime_error("No connections available");
    }

    void release_connection(TypeNumTagConnection& tntc) {
        std::lock_guard<std::mutex> lock(pool_mtx_);

        NodeType node_type;
        size_t   node_num;
        std::shared_ptr<pqxx::connection> conn;
        node_type = std::get<0>(tntc);
        node_num  = std::get<1>(tntc);
        conn      = std::move(std::get<3>(tntc));
        if (pool_.count(node_type)) {
            if (pool_[node_type].size() > node_num) {
                pool_[node_type][node_num].conn_pool.push(std::move(conn));
            }
        }
    }
};

struct ScopedConnection
{
    std::shared_ptr<ConnectionPool>   pool{nullptr};
    std::shared_ptr<pqxx::connection> conn{nullptr};
    ConnectionPool::NodeType          node_type{ConnectionPool::NodeType::MASTER};
    size_t                            node_num{};
    std::string                       node_tag{};

    ~ScopedConnection() {
        auto tntc = std::make_tuple(node_type, node_num, node_tag, std::move(conn));
        pool->release_connection(tntc);
    }
    ScopedConnection(std::shared_ptr<ConnectionPool>& p,
                     ConnectionPool::NodeType t = ConnectionPool::NodeType::REPLICA)
    :   pool(p) {
        auto tntc = pool->get_connection(t);
        node_type = std::get<0>(tntc);
        node_num  = std::get<1>(tntc);
        node_tag  = std::get<2>(tntc);
        conn      = std::move(std::get<3>(tntc));
    }
};

} // namespace SocialNetwork
