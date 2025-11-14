#pragma once

#include <grpcpp/grpcpp.h>
#include <likes_service.grpc.pb.h>

#include "configuration.h"
#include "helpers/url.h"

class GrpcLikesClient
{
public:
    struct likes_update_rv {
        bool success{false};
        std::string error_str{};
        int32_t likes_amount{0};
    };
    struct author_friends_rv {
        bool success{false};
        std::string error_str{};
        std::vector<std::string> friend_ids{};
    };

public:
    ~GrpcLikesClient() = default;
    GrpcLikesClient(const GrpcLikesClient&) = delete;
    GrpcLikesClient(GrpcLikesClient&&) = delete;
    GrpcLikesClient& operator=(const GrpcLikesClient&) = delete;
    GrpcLikesClient& operator=(GrpcLikesClient&&) = delete;

    static GrpcLikesClient& instance()
    {
        static GrpcLikesClient singleton;
        return singleton;
    }

    likes_update_rv get_likes_amount(const std::string& post_id);
    likes_update_rv increment_likes(const std::string& post_id, const std::string& saga_id);
    likes_update_rv decrement_likes(const std::string& post_id, const std::string& saga_id);
    author_friends_rv get_author_friends(const std::string& post_id, const std::string& saga_id);

private:
    GrpcLikesClient()
    :   logger_(Configuration::instance().get_logger())
    {
        const Configuration& configuration = Configuration::instance();
        UrlHelpers::Url url(configuration.grpc_url);
        channel_ = grpc::CreateChannel(std::format("{}:{}", url.get_host(), url.get_port()), grpc::InsecureChannelCredentials()); // без SSL/TLS
        stub_    = social_network::likes::LikesService::NewStub(channel_);
    }

private:
    std::shared_ptr<Logging::Logger>                           logger_{nullptr};
    std::shared_ptr<grpc::Channel>                             channel_{nullptr};
    std::unique_ptr<social_network::likes::LikesService::Stub> stub_{nullptr};
};
