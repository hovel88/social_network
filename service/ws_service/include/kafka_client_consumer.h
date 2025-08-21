#pragma once

#include <unordered_set>
#include <shared_mutex>
#include <functional>
#include <memory>
#include <thread>
#include <librdkafka/rdkafkacpp.h>
#include "configuration.h"

class KafkaConsumer
{
public:
    using MessageHandlerFunc = std::function<void(const std::string&, const std::string&)>;

    class EventReporter : public RdKafka::EventCb
    {
    public:
        virtual ~EventReporter() = default;
        EventReporter();

        void event_cb(RdKafka::Event& event);

    private:
        std::shared_ptr<Logging::Logger> logger_{nullptr};
    };

public:
    ~KafkaConsumer();
    KafkaConsumer();
    KafkaConsumer(const KafkaConsumer&) = delete;
    KafkaConsumer(KafkaConsumer&&) = delete;
    KafkaConsumer& operator=(const KafkaConsumer&) = delete;
    KafkaConsumer& operator=(KafkaConsumer&&) = delete;

    void initialize(const std::string& brokers);
    void subscribe_to_user_topic(const std::string& user_id);
    void unsubscribe_from_user_topic(const std::string& user_id);
    void start_consuming();
    void stop_consuming();

    bool is_message_handler_assigned() const;
    void assign_message_handler(MessageHandlerFunc handler);

private:
    std::shared_ptr<Logging::Logger>        logger_{nullptr};
    std::unique_ptr<RdKafka::KafkaConsumer> consumer_{nullptr};

    std::atomic<bool> running_{false};
    std::thread       thread_{};

    std::unordered_set<std::string> subscribed_topics_{};
    std::shared_mutex               subscribed_topics_mtx_{};

    std::atomic<bool>  handler_assigned_{false};
    MessageHandlerFunc handler_{};

    EventReporter reporter_{};
    int64_t       msg_bytes_{0};
    int64_t       msg_cnt_{0};
};
