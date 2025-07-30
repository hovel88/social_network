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
    struct auth_rv {
        std::string error_str{};
        bool        authenticated{false};
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

private:
    using Buf_t = tnt::Buffer<16 * 1024>;
    using Net_t = LibevNetProvider<Buf_t, DefaultStream>;

    std::shared_ptr<Logging::Logger>          logger_{nullptr};
    std::shared_ptr<Configuration>            conf_{nullptr};
    std::shared_ptr<Connector<Buf_t, Net_t>>  client_{nullptr};
    std::shared_ptr<Connection<Buf_t, Net_t>> connection_{nullptr};
};
