#pragma once

#include <unordered_map>
#include <mutex>
#include <drogon/drogon.h>
#include <drogon/WebSocketController.h>
#include "configuration.h"

class PostFeedWebSocket : public drogon::WebSocketController<PostFeedWebSocket>
{
public:
    virtual ~PostFeedWebSocket() = default;
    PostFeedWebSocket()
    :   logger_(Configuration::instance().get_logger()) {}

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/post/feed/posted", drogon::Get);
    WS_PATH_LIST_END

    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                          std::string&& message,
                          const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& req,
                             const drogon::WebSocketConnectionPtr& conn) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::mutex                                                      connections_mtx_{};
    std::unordered_map<std::string, drogon::WebSocketConnectionPtr> connections_{};
};
