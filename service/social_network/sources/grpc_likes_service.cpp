#include <google/protobuf/util/json_util.h>
#include "grpc_likes_service.h"

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

grpc::Status GrpcLikesService::GetLikesCountMessage(grpc::ServerContext* /*context*/,
                                                    const social_network::likes::GetLikesCountMessageRequest* request,
                                                    social_network::likes::GetLikesCountMessageResponse* response)
{
    LOGGER_TRACE(std::format("GetLikesCountMessage: recv Protobuf: {}", proto_message_to_str_(*request)));
    const std::string post_id{request->post_id()};

    std::string err{};
    try {
        auto rv = db_->get_post_likes(post_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            response->set_success(true);
            response->set_count(rv.likes.value());
            LOGGER_TRACE(std::format("GetLikesCountMessage: resp Protobuf: {}", proto_message_to_str_(*response)));
            return grpc::Status::OK;
        }
    } catch (std::exception& ex) {
        err = std::format("GetLikesCountMessage exception: {} (post_id: {})", ex.what(), post_id);
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
    }
    response->set_success(false);
    return grpc::Status(grpc::StatusCode::INTERNAL, err);
}

grpc::Status GrpcLikesService::IncrementLikeCountMessage(grpc::ServerContext* /*context*/,
                                                         const social_network::likes::IncrementLikeCountMessageRequest* request,
                                                         social_network::likes::IncrementLikeCountMessageResponse* response)
{
    LOGGER_TRACE(std::format("IncrementLikeCountMessage: recv Protobuf: {}", proto_message_to_str_(*request)));
    const std::string post_id{request->post_id()};
    const std::string saga_id{request->saga_id()};

    std::string err{};
    try {
        auto rv = db_->inc_post_likes(post_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            response->set_success(true);
            response->set_new_count(rv.likes.value());
            LOGGER_TRACE(std::format("IncrementLikeCountMessage: resp Protobuf: {}", proto_message_to_str_(*response)));
            return grpc::Status::OK;
        }
    } catch (std::exception& ex) {
        err = std::format("IncrementLikeCountMessage exception: {} (post_id: {}, saga_id: {})", ex.what(), post_id, saga_id);
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
    }
    response->set_success(false);
    return grpc::Status(grpc::StatusCode::INTERNAL, err);
}

grpc::Status GrpcLikesService::DecrementLikeCountMessage(grpc::ServerContext* /*context*/,
                                                         const social_network::likes::DecrementLikeCountMessageRequest* request,
                                                         social_network::likes::DecrementLikeCountMessageResponse* response)
{
    LOGGER_TRACE(std::format("DecrementLikeCountMessage: recv Protobuf: {}", proto_message_to_str_(*request)));
    const std::string post_id{request->post_id()};
    const std::string saga_id{request->saga_id()};

    std::string err{};
    try {
        auto rv = db_->dec_post_likes(post_id);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            response->set_success(true);
            response->set_new_count(rv.likes.value());
            LOGGER_TRACE(std::format("DecrementLikeCountMessage: resp Protobuf: {}", proto_message_to_str_(*response)));
            return grpc::Status::OK;
        }
    } catch (std::exception& ex) {
        err = std::format("DecrementLikeCountMessage exception: {} (post_id: {}, saga_id: {})", ex.what(), post_id, saga_id);
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
    }
    response->set_success(false);
    return grpc::Status(grpc::StatusCode::INTERNAL, err);
}

grpc::Status GrpcLikesService::GetAuthorFriendsMessages(grpc::ServerContext* /*context*/,
                                                        const social_network::likes::GetAuthorFriendsMessagesRequest* request,
                                                        social_network::likes::GetAuthorFriendsMessagesResponse* response)
{
    LOGGER_TRACE(std::format("GetAuthorFriendsMessages: recv Protobuf: {}", proto_message_to_str_(*request)));
    const std::string post_id{request->post_id()};
    const std::string saga_id{request->saga_id()};

    std::string err{};
    try {
        auto rv1 = db_->get_post(post_id);
        if (!rv1.error_str.empty()) {
            err = rv1.error_str;
        } else {
            auto rv2 = db_->get_friends(rv1.post->author_user_id);
            if (!rv2.error_str.empty()) {
                err = rv2.error_str;
            } else {
                for (const auto& id : rv2.friend_ids) {
                    auto fid = response->add_friend_ids();
                    *fid = id;
                }
                LOGGER_TRACE(std::format("GetAuthorFriendsMessages: resp Protobuf: {}", proto_message_to_str_(*response)));
                return grpc::Status::OK;
            }
        }
    } catch (std::exception& ex) {
        err = std::format("GetAuthorFriendsMessages exception: {} (post_id: {}, saga_id: {})", ex.what(), post_id, saga_id);
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
    }
    return grpc::Status(grpc::StatusCode::NOT_FOUND, err);
}
