#pragma once

#include <drogon/drogon.h>
#include <drogon/WebSocketController.h>

class PostFeedWebSocket : public drogon::WebSocketController<PostFeedWebSocket>
{
public:
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/post/feed/posted", drogon::Get);
    WS_PATH_LIST_END

    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                          std::string&& message,
                          const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const drogon::HttpRequestPtr& req,
                             const drogon::WebSocketConnectionPtr& conn) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;
};
