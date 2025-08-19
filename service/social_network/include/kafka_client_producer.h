#pragma once

#include <format>
#include <drogon/drogon.h>
#include <librdkafka/rdkafkacpp.h>
#include "logger/logger.h"
#include "configuration.h"
#include "helpers/url.h"

class KafkaProducer
{
public:
    class KafkaDeliveryReporter : public RdKafka::DeliveryReportCb
    {
    private:
        std::shared_ptr<Logging::Logger> logger_{nullptr};

    public:
        void set_logger(std::shared_ptr<Logging::Logger> logger) { logger_ = logger; }

        void dr_cb(RdKafka::Message& message) override {
            // If message.err() is non-zero the message delivery failed permanently for the message
            if (message.err()) {
                LOGGER_ERROR(std::format("KafkaDeliveryReport: Message delivery failed. {}", message.errstr()));
            } else {
                LOGGER_DEBUG(std::format("KafkaDeliveryReport: Message delivered to topic '{}' [{}] at offset {}", message.topic_name(),  message.partition(), message.offset()));
            }
        }
    };

public:
    ~KafkaProducer() {
        if (producer_) {
            LOGGER_DEBUG(std::format("KafkaProducer flush: Flushing final messages..."));
            producer_->flush(1 * 1000 /* wait for max 1 second */);
            if (producer_->outq_len() > 0) {
                LOGGER_ERROR(std::format("KafkaProducer flush: {} message(s) were not delivered", producer_->outq_len()));
            }
        }
    }
    KafkaProducer() = delete;
    KafkaProducer(const KafkaProducer&) = delete;
    KafkaProducer(KafkaProducer&&) = delete;
    KafkaProducer& operator=(const KafkaProducer&) = delete;
    KafkaProducer& operator=(KafkaProducer&&) = delete;

    explicit KafkaProducer(std::shared_ptr<Logging::Logger> logger)
    :   logger_(std::move(logger)) {
        reporter_.set_logger(logger_);
        const Configuration& configuration = Configuration::instance();

        UrlHelpers::Url url(configuration.kafka_url);

        std::string errstr;
        auto conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
        if (RdKafka::Conf::CONF_OK != conf->set("bootstrap.servers", std::format("{}:{}", url.get_host(), url.get_port()), errstr)) {
            // адреса брокеров Kafka
            LOGGER_ERROR(std::format("KafkaProducer: {}", errstr));
        }
        if (RdKafka::Conf::CONF_OK != conf->set("message.timeout.ms", "5000", errstr)) {
            // таймаут отправки
            LOGGER_ERROR(std::format("KafkaProducer: {}", errstr));
        }
        if (RdKafka::Conf::CONF_OK != conf->set("queue.buffering.max.messages", "100000", errstr)) {
            LOGGER_ERROR(std::format("KafkaProducer: {}", errstr));
        }
        // if (RdKafka::Conf::CONF_OK != conf->set("compression.type", "snappy", errstr)) {
        //     // сжатие
        //     LOGGER_ERROR(std::format("KafkaProducer: {}", errstr));
        // }
        // if (RdKafka::Conf::CONF_OK != conf->set("acks", "all", errstr)) {
        //     // Ждём подтверждения от всех реплик
        //     LOGGER_ERROR(std::format("KafkaProducer: {}", errstr));
        // }
        // if (RdKafka::Conf::CONF_OK != set("retries", "5", errstr)) {
        //     LOGGER_ERROR(std::format("KafkaProducer: {}", errstr));
        // }
        // if (RdKafka::Conf::CONF_OK != set("max.in.flight.requests.per.connection", "1", errstr)) {
        //     LOGGER_ERROR(std::format("KafkaProducer: {}", errstr));
        // }
        if (RdKafka::Conf::CONF_OK != conf->set("dr_cb", &reporter_, errstr)) {
            LOGGER_ERROR(std::format("KafkaProducer: {}", errstr));
        }

        errstr.clear();
        producer_ = std::unique_ptr<RdKafka::Producer>(RdKafka::Producer::create(conf, errstr));
        if (!errstr.empty()) LOGGER_ERROR(std::format("KafkaProducer: {}", errstr));
    }

    void produce(const std::string& topic, const std::string& message) {
        if (!producer_) return;
        auto err = producer_->produce(topic,
                        RdKafka::Topic::PARTITION_UA,   // Unassigned partition. The builtin partitioner will be used to assign the message to a topic based on the message key, or random partition if the key is not set
                        RdKafka::Producer::RK_MSG_COPY, // Make a copy of the value
                        const_cast<char*>(message.data()), message.size(),
                        /*key=*/nullptr, /*key_len=*/0, // Key
                        /*timestamp=*/0,                // Timestamp (defaults to current time)
                        /*msg_opaque=*/nullptr);        // Per-message opaque value passed to delivery report
        if (err != RdKafka::ERR_NO_ERROR) {
            LOGGER_ERROR(std::format("KafkaProducer produce: Failed to produce to topic '{}': {}", topic, RdKafka::err2str(err)));
        } else {
            LOGGER_DEBUG(std::format("KafkaProducer produce: Enqueued message ({} byte(s)) for topic '{}'", message.size(), topic));
        }
        producer_->poll(0);
    }

private:
    std::shared_ptr<Logging::Logger>   logger_{nullptr};
    std::unique_ptr<RdKafka::Producer> producer_{nullptr};

    KafkaDeliveryReporter reporter_{};
};
