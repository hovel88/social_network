#include <regex>
#include "ws_controller_post.h"
#include "helpers/string.h"

static bool is_valid_uuid_(const std::string& id) {
    static const std::regex uuid_regex(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
    );
    return std::regex_match(id, uuid_regex);
}

// --------------------------------------------------------

void PostFeedWebSocket::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                                         std::string&& message,
                                         const drogon::WebSocketMessageType& type)
{
    if (type == drogon::WebSocketMessageType::Ping) {
        std::cout << "### Ping" << std::endl;
        return;
    }
    if (type == drogon::WebSocketMessageType::Pong) {
        std::cout << "### Pong" << std::endl;
        return;
    }
    if (type == drogon::WebSocketMessageType::Close) {
        std::cout << "### Close" << std::endl;
        return;
    }
    if (type != drogon::WebSocketMessageType::Text) {
        std::cout << "### Unsupported message type " << (int)type << std::endl;
        return;
    }
    std::cout << "### new message=" << message << std::endl;
    conn->send(std::string("new message=") + message);
}

void PostFeedWebSocket::handleNewConnection(const drogon::HttpRequestPtr& req,
                                            const drogon::WebSocketConnectionPtr& conn)
{
    bool authenticated = false;

    std::string user_id{};
    auto auth_header = req->getHeader("Authorization");
    if (!auth_header.empty()
    &&  auth_header.find("Bearer ") == 0) {
        user_id = StringHelpers::to_lowercase(auth_header.substr(7));
        if (is_valid_uuid_(user_id)) {
            conn->setContext(std::make_shared<std::string>(user_id));
            authenticated = true;
        }
    }
    if (!authenticated
    ||  user_id.empty()) {
        std::cout << "### connected, not authenticated, conn->forceClose()" << std::endl;
        conn->forceClose();
        return;
    }

    std::string instance_addr = conn->localAddr().toIpPort();

    std::cout << "### connected, authenticated, instance_addr=" << instance_addr << ", user_id=" << user_id << std::endl;
    conn->send(std::string("connected:: instance_addr=") + instance_addr + std::string(", user_id=") + user_id);
}

void PostFeedWebSocket::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    std::string instance_addr = conn->localAddr().toIpPort();

    std::string user_id{};
    user_id = conn->hasContext() ? (conn->getContextRef<std::string>()) : ("unknown");

    std::cout << "### disconnected, instance_addr=" << instance_addr << ", user_id=" << user_id << std::endl;
    conn->send(std::string("disconnected:: instance_addr=") + instance_addr + std::string(", user_id=") + user_id);
}
