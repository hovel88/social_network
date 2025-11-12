#include <regex>
#include "helpers/string.h"
#include "ws_controller_post.h"
#include "kafka_plugin.h"

static bool is_valid_uuid_(const std::string& id) {
    static const std::regex uuid_regex(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
    );
    return std::regex_match(id, uuid_regex);
}

// --------------------------------------------------------

void PostFeedWebSocket::handleNewMessage(const drogon::WebSocketConnectionPtr& /*conn*/,
                                         std::string&& /*message*/,
                                         const drogon::WebSocketMessageType& /*type*/)
{
    // if (type == drogon::WebSocketMessageType::Ping) {
    //     std::cout << "### Ping" << std::endl;
    //     return;
    // }
    // if (type == drogon::WebSocketMessageType::Pong) {
    //     std::cout << "### Pong" << std::endl;
    //     return;
    // }
    // if (type == drogon::WebSocketMessageType::Close) {
    //     std::cout << "### Close" << std::endl;
    //     return;
    // }
    // if (type != drogon::WebSocketMessageType::Text) {
    //     std::cout << "### Unsupported message type " << (int)type << std::endl;
    //     return;
    // }
    // std::cout << "### new message=" << message << std::endl;
}

void PostFeedWebSocket::handleNewConnection(const drogon::HttpRequestPtr& req,
                                            const drogon::WebSocketConnectionPtr& conn)
{
    std::string instance_addr = conn->localAddr().toIpPort();

    auto auth_header = req->getHeader("Authorization");
    if (auth_header.empty()
    ||  auth_header.find("Bearer ") != 0) {
        LOGGER_ERROR(std::format("<instance_addr={}> websocket: NOT authenticated, force close connection", instance_addr));
        conn->forceClose();
        return;
    }

    const std::string user_id = StringHelpers::to_lowercase(auth_header.substr(7));
    if (!is_valid_uuid_(user_id)) {
        LOGGER_ERROR(std::format("<instance_addr={}> websocket: NOT authenticated, force close connection", instance_addr));
        conn->forceClose();
        return;
    }

    auto kafka_plugin = drogon::app().getSharedPlugin<KafkaPlugin>();
    if (!kafka_plugin) {
        LOGGER_ERROR(std::format("<instance_addr={}> websocket: user_id={}, no Kafka plugin available, force close connection", instance_addr, user_id));
        conn->forceClose();
        return;
    }
    auto kafka_consumer = kafka_plugin->get_consumer();
    if (!kafka_consumer) {
        LOGGER_ERROR(std::format("<instance_addr={}> websocket: user_id={}, no Kafka consumer, force close connection", instance_addr, user_id));
        conn->forceClose();
        return;
    }

    if (!kafka_consumer->is_message_handler_assigned()) {
        kafka_consumer->assign_message_handler([this](const std::string& to_user_id, const std::string& message) {
            std::lock_guard<std::mutex> lock(connections_mtx_);
            if (connections_.count(to_user_id)) {
                connections_[to_user_id]->send(message);
            }
        });
    }

    LOGGER_INFOR(std::format("<instance_addr={}> websocket: user_id={}, start processing", instance_addr, user_id));
    kafka_consumer->subscribe_to_user_topic(user_id);
    kafka_consumer->subscribe_to_likes_topic(user_id);

    // сохраняем соединение
    std::lock_guard lock(connections_mtx_);
    connections_[user_id] = conn;
    conn->setContext(std::make_shared<std::string>(user_id));
}

void PostFeedWebSocket::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    std::string instance_addr = conn->localAddr().toIpPort();

    if (conn->hasContext()) {
        const std::string user_id = conn->getContextRef<std::string>();

        auto kafka_plugin   = drogon::app().getSharedPlugin<KafkaPlugin>();
        auto kafka_consumer = kafka_plugin->get_consumer();

        LOGGER_INFOR(std::format("<instance_addr={}> websocket: user_id={}, stop processing", instance_addr, user_id));
        kafka_consumer->unsubscribe_from_user_topic(user_id);
        kafka_consumer->unsubscribe_from_likes_topic(user_id);

        // удаляем соединение
        std::lock_guard lock(connections_mtx_);
        connections_.erase(user_id);
    }
}
