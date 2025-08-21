#pragma once

#include <librdkafka/rdkafkacpp.h>
#include "configuration.h"

class KafkaProducer
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
    ~KafkaProducer();
    KafkaProducer();
    KafkaProducer(const KafkaProducer&) = delete;
    KafkaProducer(KafkaProducer&&) = delete;
    KafkaProducer& operator=(const KafkaProducer&) = delete;
    KafkaProducer& operator=(KafkaProducer&&) = delete;

    void initialize(const std::string& brokers);
    void produce(const std::string& topic, const std::string& message);
    void produce_to_user_topics(const std::vector<std::string>& friend_ids,
                                const std::string& message);

private:
    std::shared_ptr<Logging::Logger>   logger_{nullptr};
    std::unique_ptr<RdKafka::Producer> producer_{nullptr};

    KafkaDeliveryReporter reporter_{};
};
