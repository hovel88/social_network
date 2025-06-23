#pragma once

#include <cxxopts.hpp>
#include "configuration/configuration_data.h"
#include "logger/logger.h"

namespace SocialNetwork {

std::shared_ptr<cxxopts::ParseResult> configure_cli_options(int argc, char** argv);



class Configuration
{
public:
    ~Configuration() = default;
    Configuration() = delete;
    Configuration(const Configuration&) = delete;
    Configuration(Configuration&&) = delete;
    Configuration& operator=(const Configuration&) = delete;
    Configuration& operator=(Configuration&&) = delete;

    explicit Configuration(std::shared_ptr<Logging::Logger> logger,
                           std::shared_ptr<cxxopts::ParseResult> cli_opts)
    :   logger_(std::move(logger)) {
        apply_(std::move(cli_opts));
    }

    const config_data::config_s& config() const noexcept;

    void show_configuration() const;

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};

    config_data::config_s current_configuration_;

    void apply_(std::shared_ptr<cxxopts::ParseResult> cli_opts);
};

} // namespace SocialNetwork
