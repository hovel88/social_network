#pragma once

#include <drogon/drogon.h>
#include <drogon/plugins/Plugin.h>
#include <nlohmann/json.hpp>
#include "helpers/url.h"
#include "kafka_client_consumer.h"
#include "configuration.h"

class KafkaPlugin : public drogon::Plugin<KafkaPlugin>
{
public:
    virtual ~KafkaPlugin() = default;
    KafkaPlugin()
    :   logger_(Configuration::instance().get_logger()),
        consumer_(std::make_shared<KafkaConsumer>()) {}

    // вызывается внутри drogon при старте плагина
    void initAndStart(const Json::Value& /*config*/) override
    {
        const Configuration& configuration = Configuration::instance();

        UrlHelpers::Url url(configuration.kafka_url);
        consumer_->initialize(std::format("{}:{}", url.get_host(), url.get_port()));
        consumer_->start_consuming();
    }

    // вызывается внутри drogon при остановке плагина
    void shutdown() override
    {
        if (consumer_) {
            consumer_->stop_consuming();
        }
    }

    std::shared_ptr<KafkaConsumer> get_consumer() { return consumer_; }

private:
    std::shared_ptr<Logging::Logger> logger_{nullptr};
    std::shared_ptr<KafkaConsumer>   consumer_{nullptr};
};
