#pragma once

#include "configuration/configuration_data.h"
#include "logger/logger.h"

namespace SocialNetwork {

class Configuration
{
public:
    ~Configuration() = default;
    Configuration() = delete;
    Configuration(const Configuration&) = delete;
    Configuration(Configuration&&) = delete;
    Configuration& operator=(const Configuration&) = delete;
    Configuration& operator=(Configuration&&) = delete;

    explicit Configuration(std::shared_ptr<Logging::Logger> logger)
    :   logger_(std::move(logger)) {
        apply_();
    }

    const config_data::config_s& config() const noexcept;

    void show_configuration() const;

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};

    config_data::config_s current_configuration_;

    void apply_();
};

} // namespace SocialNetwork
