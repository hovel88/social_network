#pragma once

#include <librdkafka/rdkafkacpp.h>

#include "configuration.h"
#include "helpers/url.h"

class KafkaLikesProducer
{
public:
    class KafkaDeliveryReporter : public RdKafka::DeliveryReportCb
    {
    public:
        virtual ~KafkaDeliveryReporter() = default;
        KafkaDeliveryReporter();

        void dr_cb(RdKafka::Message& message) override;

    private:
        std::shared_ptr<Logging::Logger> logger_{nullptr};
    };

public:
    ~KafkaLikesProducer();
    KafkaLikesProducer(const KafkaLikesProducer&) = delete;
    KafkaLikesProducer(KafkaLikesProducer&&) = delete;
    KafkaLikesProducer& operator=(const KafkaLikesProducer&) = delete;
    KafkaLikesProducer& operator=(KafkaLikesProducer&&) = delete;

    static KafkaLikesProducer& instance()
    {
        static KafkaLikesProducer singleton;
        return singleton;
    }

    void produce(const std::string& topic, const std::string& message);
    void produce_to_user_topics(const std::vector<std::string>& friend_ids,
                                const std::string& message);

private:
    KafkaLikesProducer()
    :   logger_(Configuration::instance().get_logger())
    {
        const Configuration& configuration = Configuration::instance();
        UrlHelpers::Url url(configuration.kafka_url);
        initialize(std::format("{}:{}", url.get_host(), url.get_port()));
    }

    void initialize(const std::string& brokers);

private:
    std::shared_ptr<Logging::Logger>   logger_{nullptr};
    std::unique_ptr<RdKafka::Producer> producer_{nullptr};

    KafkaDeliveryReporter reporter_{};
};
