#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <grpcpp/grpcpp.h>
#include <chat_service.grpc.pb.h>
#include "logger/logger.h"
#include "app_inmem_service.h"

class GrpcDialogService final : public social_network::chats::DialogService::Service
{
public:
    virtual ~GrpcDialogService() = default;
    GrpcDialogService() = delete;
    GrpcDialogService(const GrpcDialogService&) = delete;
    GrpcDialogService(GrpcDialogService&&) = delete;
    GrpcDialogService& operator=(const GrpcDialogService&) = delete;
    GrpcDialogService& operator=(GrpcDialogService&&) = delete;

    explicit GrpcDialogService(std::shared_ptr<Logging::Logger> logger,
                               std::shared_ptr<InMemService> inmem)
    :   logger_(std::move(logger)),
        inmem_(std::move(inmem)) {}

    virtual grpc::Status SendDialogMessage(grpc::ServerContext* context,
                                           const social_network::chats::SendDialogMessageRequest* request,
                                           social_network::chats::SendDialogMessageResponse* response) override;

    virtual grpc::Status ListDialogMessages(grpc::ServerContext* context,
                                            const social_network::chats::ListDialogMessagesRequest* request,
                                            social_network::chats::ListDialogMessagesResponse* response) override;

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<InMemService>    inmem_{nullptr};
};
