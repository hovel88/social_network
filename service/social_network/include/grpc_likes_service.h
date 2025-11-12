#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <grpcpp/grpcpp.h>
#include <likes_service.grpc.pb.h>
#include "logger/logger.h"
#include "app_database_service.h"

class GrpcLikesService final : public social_network::likes::LikesService::Service
{
public:
    virtual ~GrpcLikesService() = default;
    GrpcLikesService() = delete;
    GrpcLikesService(const GrpcLikesService&) = delete;
    GrpcLikesService(GrpcLikesService&&) = delete;
    GrpcLikesService& operator=(const GrpcLikesService&) = delete;
    GrpcLikesService& operator=(GrpcLikesService&&) = delete;

    explicit GrpcLikesService(std::shared_ptr<Logging::Logger> logger,
                              std::shared_ptr<DatabaseService> db)
    :   logger_(std::move(logger)),
        db_(std::move(db)) {}

    virtual grpc::Status IncrementLikeCountMessage(grpc::ServerContext* context,
                                                   const social_network::likes::IncrementLikeCountMessageRequest* request,
                                                   social_network::likes::IncrementLikeCountMessageResponse* response) override;

    virtual grpc::Status DecrementLikeCountMessage(grpc::ServerContext* context,
                                                   const social_network::likes::DecrementLikeCountMessageRequest* request,
                                                   social_network::likes::DecrementLikeCountMessageResponse* response) override;

    virtual grpc::Status GetAuthorFriendsMessages(grpc::ServerContext* context,
                                                  const social_network::likes::GetAuthorFriendsMessagesRequest* request,
                                                  social_network::likes::GetAuthorFriendsMessagesResponse* response) override;

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<DatabaseService> db_{nullptr};
};
