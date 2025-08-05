#include <format>
#include "app_inmem_service.h"
#include "helpers/url.h"
#include "helpers/ip_address.h"
#include "helpers/socket_address.h"

InMemService::~InMemService()
{
    if (client_ != nullptr) {
        if (connection_ != nullptr) {
            client_->close(*connection_);
        }
    }
}

InMemService::InMemService(std::shared_ptr<Logging::Logger> logger,
                           std::shared_ptr<Configuration> conf)
:   logger_(std::move(logger)),
    conf_(std::move(conf)),
    client_(std::make_shared<Connector<Buf_t, Net_t>>()),
    connection_(std::make_shared<Connection<Buf_t, Net_t>>(*client_))
{
    UrlHelpers::Url url(conf_->config().tarantool.url);

    int rc = client_->connect(*connection_, {
        .address = url.get_host(),                      //"tarantool_db"
        .service = std::to_string(url.get_port()),      //std::to_string(3301)
        .user    = conf_->config().tarantool.login,     //"tntuser"
        .passwd  = conf_->config().tarantool.password   //"tntpass"
    });
    if (rc != 0) {
        LOGGER_ERROR(std::format("Tarantool -- {}", connection_->getError().msg));
        connection_.reset();
    }
}

InMemService::auth_rv InMemService::authenticate_user(const std::string& user_id)
{
    auth_rv rv{};
    if (client_ && connection_) {
        std::vector<Auth> results{};

        try {
            rid_t check_future = connection_->call("check_user", std::make_tuple(user_id));
            while (!connection_->futureIsReady(check_future)) {
                if (client_->wait(*connection_, check_future, 1000) != 0) {
                    LOGGER_ERROR(std::format("Tarantool -- {}", connection_->getError().msg));
                    connection_->reset();
                }
            }
            std::optional<Response<Buf_t>> resp_opt = connection_->getResponse(check_future);
            if (resp_opt.has_value()) {
                auto& response = *resp_opt;
                if (response.body.error_stack.has_value()) {
                    for (const auto& err : response.body.error_stack.value()) {
                        LOGGER_ERROR(std::format("Tarantool -- RESPONSE ERROR: msg='{}' line='{}' errno='{}' type='{}' code='{}'",
                            err.msg, err.file, err.file, err.saved_errno, err.type_name, err.errcode));
                    }
                }
                if (response.body.data.has_value()) {
                    bool ok = response.body.data->decode(results);
                    if (!ok) {
                        LOGGER_ERROR(std::format("Tarantool -- data.decode : false"));
                    }
                    if (results.empty()) {
                        LOGGER_ERROR(std::format("Tarantool -- data.decode : empty"));
                    }
                    // for (auto const& result : results) {
                    //     LOGGER_DEBUG(std::format("Tarantool -- data.decode : {}", result.to_string()));
                    // }
                }
            }

            rv.error_str.clear();
            rv.authenticated = !results.empty();
        } catch (std::exception& ex) {
            rv.error_str = std::format("Tarantool exception: {}", ex.what());
            rv.authenticated = false;
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to Tarantool");
        rv.authenticated = false;
    }
    return rv;
}

InMemService::common_rv InMemService::send_dialog_message(const std::string& from_id, const std::string& to_id, const std::string& message)
{
    common_rv rv{};
    if (client_ && connection_) {
        std::vector<Common> results{};

        try {
            rid_t send_future = connection_->call("send_dialog_message", std::make_tuple(from_id, to_id, message));
            while (!connection_->futureIsReady(send_future)) {
                if (client_->wait(*connection_, send_future, 1000) != 0) {
                    LOGGER_ERROR(std::format("Tarantool -- {}", connection_->getError().msg));
                    connection_->reset();
                }
            }
            std::optional<Response<Buf_t>> resp_opt = connection_->getResponse(send_future);
            if (resp_opt.has_value()) {
                auto& response = *resp_opt;
                if (response.body.error_stack.has_value()) {
                    for (const auto& err : response.body.error_stack.value()) {
                        LOGGER_ERROR(std::format("Tarantool -- RESPONSE ERROR: msg='{}' line='{}' errno='{}' type='{}' code='{}'",
                            err.msg, err.file, err.file, err.saved_errno, err.type_name, err.errcode));
                    }
                }
                if (response.body.data.has_value()) {
                    bool ok = response.body.data->decode(results);
                    if (!ok) {
                        LOGGER_ERROR(std::format("Tarantool -- data.decode : false"));
                    }
                    if (results.empty()) {
                        LOGGER_ERROR(std::format("Tarantool -- data.decode : empty"));
                    }
                    // for (auto const& result : results) {
                    //     LOGGER_DEBUG(std::format("Tarantool -- data.decode : {}", result.to_string()));
                    // }
                }
            }

            rv.error_str.clear();
        } catch (std::exception& ex) {
            rv.error_str = std::format("Tarantool exception: {}", ex.what());
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to Tarantool");
    }
    return rv;
}

InMemService::dialog_rv InMemService::list_dialog_messages(const std::string& from_id, const std::string& to_id, uint32_t limit)
{
    dialog_rv rv{};
    if (client_ && connection_) {
        std::vector<Message> results{};

        try {
            rid_t list_future = connection_->call("list_dialog_messages", std::make_tuple(from_id, to_id, limit));
            while (!connection_->futureIsReady(list_future)) {
                if (client_->wait(*connection_, list_future, 1000) != 0) {
                    LOGGER_ERROR(std::format("Tarantool -- {}", connection_->getError().msg));
                    connection_->reset();
                }
            }
            std::optional<Response<Buf_t>> resp_opt = connection_->getResponse(list_future);
            if (resp_opt.has_value()) {
                auto& response = *resp_opt;
                if (response.body.error_stack.has_value()) {
                    for (const auto& err : response.body.error_stack.value()) {
                        LOGGER_ERROR(std::format("Tarantool -- RESPONSE ERROR: msg='{}' line='{}' errno='{}' type='{}' code='{}'",
                            err.msg, err.file, err.file, err.saved_errno, err.type_name, err.errcode));
                    }
                }
                if (response.body.data.has_value()) {
                    bool ok = response.body.data->decode(results);
                    if (!ok) {
                        LOGGER_ERROR(std::format("Tarantool -- data.decode : false"));
                    }
                    if (results.empty()) {
                        LOGGER_ERROR(std::format("Tarantool -- data.decode : empty"));
                    }
                    // for (auto const& result : results) {
                    //     LOGGER_DEBUG(std::format("Tarantool -- data.decode : {}", result.to_string()));
                    // }
                }
            }

            rv.error_str.clear();
            rv.messages = results;
        } catch (std::exception& ex) {
            rv.error_str = std::format("Tarantool exception: {}", ex.what());
        }
    } else {
        rv.error_str = std::format("server error: there is no connection to Tarantool");
    }
    return rv;
}
