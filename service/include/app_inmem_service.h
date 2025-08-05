#pragma once

#include <optional>
#include <vector>
#include <Buffer/Buffer.hpp>
#include <Client/Connector.hpp>
#include <Client/LibevNetProvider.hpp>
#include "logger/logger.h"
#include "configuration/configuration.h"

class InMemService
{
public:
    struct Common {
        std::string ok;

        static constexpr auto mpp = std::make_tuple(&Common::ok);
        std::string to_string() const {
            return std::string("ok=") + ok;
        }
    };
    struct Auth {
        std::string id;
        std::string pwd_hash;

        static constexpr auto mpp = std::make_tuple(&Auth::id, &Auth::pwd_hash);
        std::string to_string() const {
            return std::string("id=") + id + std::string("  pwd_hash=") + pwd_hash;
        }
    };
    struct Message {
        std::string from{};
        std::string to{};
        std::string text{};
        uint64_t    created_at_msec{};

        static constexpr auto mpp = std::make_tuple(&Message::from, &Message::to, &Message::text, &Message::created_at_msec);
        std::string to_string() const {
            return std::string("from=") + from + std::string("  to=") + to + std::string("  created_at=") + std::to_string(created_at_msec);
        }
    };

    struct common_rv {
        std::string error_str{};
    };
    struct auth_rv {
        std::string error_str{};
        bool        authenticated{false};
    };
    struct dialog_rv {
        std::string error_str{};
        std::vector<Message> messages{};
    };

public:
    ~InMemService();
    InMemService() = delete;
    InMemService(const InMemService&) = delete;
    InMemService(InMemService&&) = delete;
    InMemService& operator=(const InMemService&) = delete;
    InMemService& operator=(InMemService&&) = delete;

    explicit InMemService(std::shared_ptr<Logging::Logger> logger, std::shared_ptr<Configuration> conf);

    auth_rv authenticate_user(const std::string& user_id);

    common_rv send_dialog_message(const std::string& from_id, const std::string& to_id, const std::string& message);
    dialog_rv list_dialog_messages(const std::string& from_id, const std::string& to_id, uint32_t limit);

private:
    using Buf_t = tnt::Buffer<16 * 1024 * 1024>;
    using Net_t = LibevNetProvider<Buf_t, DefaultStream>;

    std::shared_ptr<Logging::Logger>          logger_{nullptr};
    std::shared_ptr<Configuration>            conf_{nullptr};
    std::shared_ptr<Connector<Buf_t, Net_t>>  client_{nullptr};
    std::shared_ptr<Connection<Buf_t, Net_t>> connection_{nullptr};
};
