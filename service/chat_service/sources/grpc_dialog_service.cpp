#include "grpc_dialog_service.h"

grpc::Status GrpcDialogService::SendDialogMessage(grpc::ServerContext* /*context*/,
                                                  const social_network::chats::SendDialogMessageRequest* request,
                                                  social_network::chats::SendDialogMessageResponse* response)
{
    const std::string message{request->message().text()};
    const std::string to_user_id{request->message().to_user_id()};
    const std::string from_user_id{request->message().from_user_id()};

    std::string err{};
    try {
        auto rv = inmem_->send_dialog_message(from_user_id, to_user_id, message);
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            response->set_success(true);
            return grpc::Status::OK;
        }
    } catch (std::exception& ex) {
        err = std::format("SendDialogMessage exception: {} (from_user_id: {}, to_user_id: {})", ex.what(), from_user_id, to_user_id);
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
    }
    response->set_success(false);
    return grpc::Status(grpc::StatusCode::INTERNAL, err);
}

grpc::Status GrpcDialogService::ListDialogMessages(grpc::ServerContext* /*context*/,
                                                   const social_network::chats::ListDialogMessagesRequest* request,
                                                   social_network::chats::ListDialogMessagesResponse* response)
{
    const std::string to_user_id{request->to_user_id()};
    const std::string from_user_id{request->from_user_id()};

    std::string err{};
    try {
        auto rv = inmem_->list_dialog_messages(from_user_id, to_user_id, request->limit());
        if (!rv.error_str.empty()) {
            err = rv.error_str;
        } else {
            for (const auto& m : rv.messages) {
                auto message = response->add_messages();
                message->set_from_user_id(m.from);
                message->set_to_user_id(m.to);
                message->set_text(m.text);
            }
            return grpc::Status::OK;
        }
    } catch (std::exception& ex) {
        err = std::format("ListDialogMessages exception: {} (from_user_id: {}, to_user_id: {})", ex.what(), from_user_id, to_user_id);
    }

    if (!err.empty()) {
        LOGGER_ERROR(err);
    }
    return grpc::Status(grpc::StatusCode::NOT_FOUND, err);
}
