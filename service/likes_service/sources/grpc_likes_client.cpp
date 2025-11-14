#include "grpc_likes_client.h"

#include <format>
#include <nlohmann/json.hpp>
#include <google/protobuf/util/json_util.h>

static std::string proto_message_to_str_(const google::protobuf::Message& obj)
{
    std::string str;
    google::protobuf::util::JsonPrintOptions opt;
    opt.always_print_primitive_fields = true;
    if (!google::protobuf::util::MessageToJsonString(obj, &str, opt).ok()) {
        str.assign("can't convert proto");
    }
    return str;
}

// --------------------------------------------------------

GrpcLikesClient::likes_update_rv GrpcLikesClient::get_likes_amount(const std::string& post_id)
{
    likes_update_rv rv{};
    try {
        social_network::likes::GetLikesCountMessageResponse response;
        social_network::likes::GetLikesCountMessageRequest request;
        request.set_post_id(post_id);
        LOGGER_TRACE(std::format("get_likes_amount: send Protobuf: {}", proto_message_to_str_(request)));

        grpc::ClientContext context;
        grpc::Status status = stub_->GetLikesCountMessage(&context, request, &response);
        if (!status.ok()) {
            rv.error_str = status.error_message();
            rv.success   = false;
        } else {
            LOGGER_TRACE(std::format("get_likes_amount: recv Protobuf: {}", proto_message_to_str_(response)));

            rv.error_str.clear();
            rv.success      = response.success();
            rv.likes_amount = response.count();
        }
    } catch (std::exception& ex) {
        rv.error_str = std::format("get_likes_amount exception: {} (post_id: {})", ex.what(), post_id);
        rv.success   = false;
    }

    return rv;
}

GrpcLikesClient::likes_update_rv GrpcLikesClient::increment_likes(const std::string& post_id, const std::string& saga_id)
{
    likes_update_rv rv{};
    try {
        social_network::likes::IncrementLikeCountMessageResponse response;
        social_network::likes::IncrementLikeCountMessageRequest request;
        request.set_post_id(post_id);
        request.set_saga_id(saga_id);
        LOGGER_TRACE(std::format("increment_likes_handler: send Protobuf: {}", proto_message_to_str_(request)));

        grpc::ClientContext context;
        grpc::Status status = stub_->IncrementLikeCountMessage(&context, request, &response);
        if (!status.ok()) {
            rv.error_str = status.error_message();
            rv.success   = false;
        } else {
            LOGGER_TRACE(std::format("increment_likes_handler: recv Protobuf: {}", proto_message_to_str_(response)));

            rv.error_str.clear();
            rv.success      = response.success();
            rv.likes_amount = response.new_count();
        }
    } catch (std::exception& ex) {
        rv.error_str = std::format("increment_likes_handler exception: {} (post_id: {}, saga_id: {})", ex.what(), post_id, saga_id);
        rv.success   = false;
    }

    return rv;
}

GrpcLikesClient::likes_update_rv GrpcLikesClient::decrement_likes(const std::string& post_id, const std::string& saga_id)
{
    likes_update_rv rv{};
    try {
        social_network::likes::DecrementLikeCountMessageResponse response;
        social_network::likes::DecrementLikeCountMessageRequest request;
        request.set_post_id(post_id);
        request.set_saga_id(saga_id);
        LOGGER_TRACE(std::format("decrement_likes_handler: send Protobuf: {}", proto_message_to_str_(request)));

        grpc::ClientContext context;
        grpc::Status status = stub_->DecrementLikeCountMessage(&context, request, &response);
        if (!status.ok()) {
            rv.error_str = status.error_message();
            rv.success   = false;
        } else {
            LOGGER_TRACE(std::format("decrement_likes_handler: recv Protobuf: {}", proto_message_to_str_(response)));

            rv.error_str.clear();
            rv.success      = response.success();
            rv.likes_amount = response.new_count();
        }
    } catch (std::exception& ex) {
        rv.error_str = std::format("decrement_likes_handler exception: {} (post_id: {}, saga_id: {})", ex.what(), post_id, saga_id);
        rv.success   = false;
    }

    return rv;
}

GrpcLikesClient::author_friends_rv GrpcLikesClient::get_author_friends(const std::string& post_id, const std::string& saga_id)
{
    author_friends_rv rv{};
    try {
        social_network::likes::GetAuthorFriendsMessagesResponse response;
        social_network::likes::GetAuthorFriendsMessagesRequest request;
        request.set_post_id(post_id);
        request.set_saga_id(saga_id);
        LOGGER_TRACE(std::format("get_author_friends_handler: send Protobuf: {}", proto_message_to_str_(request)));

        grpc::ClientContext context;
        grpc::Status status = stub_->GetAuthorFriendsMessages(&context, request, &response);
        if (!status.ok()) {
            rv.error_str = status.error_message();
            rv.success   = false;
        } else {
            LOGGER_TRACE(std::format("get_author_friends_handler: recv Protobuf: {}", proto_message_to_str_(response)));

            rv.error_str.clear();
            rv.success      = true;
            for (int i = 0; i < response.friend_ids_size(); ++i) {
                rv.friend_ids.emplace_back(response.friend_ids(i));
            }
        }
    } catch (std::exception& ex) {
        rv.error_str = std::format("get_author_friends_handler exception: {} (post_id: {}, saga_id: {})", ex.what(), post_id, saga_id);
        rv.success   = false;
    }

    return rv;
}
