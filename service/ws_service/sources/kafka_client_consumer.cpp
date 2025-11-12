#include "helpers/environment.h"
#include "kafka_client_consumer.h"

KafkaConsumer::EventReporter::EventReporter()
:   logger_(Configuration::instance().get_logger())
{
}

void KafkaConsumer::EventReporter::event_cb(RdKafka::Event& event)
{
    switch (event.type()) {
    case RdKafka::Event::EVENT_ERROR:
        LOGGER_TRACE(std::format("EventReporter: {} ERROR ({}): {}", (event.fatal() ? "FATAL " : ""), RdKafka::err2str(event.err()), event.str()));
        break;
    case RdKafka::Event::EVENT_STATS:
        LOGGER_TRACE(std::format("EventReporter: STATS: {}", event.str()));
        break;
    case RdKafka::Event::EVENT_LOG:
        LOGGER_TRACE(std::format("EventReporter: LOG-{}-{}: {}", (int)event.severity(), event.fac(), event.str()));
        break;
    case RdKafka::Event::EVENT_THROTTLE:
        LOGGER_TRACE(std::format("EventReporter: THROTTLED: {}ms by {} id {}", event.throttle_time(), event.broker_name(), event.broker_id()));
        break;
    default:
        LOGGER_TRACE(std::format("EventReporter: EVENT {} ({}): {}", (int)event.type(), RdKafka::err2str(event.err()), event.str()));
        break;
    }
}

// --------------------------------------------------------

KafkaConsumer::~KafkaConsumer()
{
    stop_consuming();
    LOGGER_DEBUG(std::format("KafkaConsumer: consumed {} message(s) ({} byte(s))", msg_cnt_, msg_bytes_));
    RdKafka::wait_destroyed(1000);
}

KafkaConsumer::KafkaConsumer()
:   logger_(Configuration::instance().get_logger())
{
}

void KafkaConsumer::initialize(const std::string& brokers)
{
    std::string errstr;
    auto conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    conf->set("bootstrap.servers", brokers, errstr);
    conf->set("client.id", EnvironmentHelpers::node_name(), errstr);
    conf->set("group.id", "ws_service_group", errstr);
    conf->set("enable.auto.commit", "true", errstr);
    conf->set("auto.commit.interval.ms", "1000", errstr);
    conf->set("enable.partition.eof", "false", errstr);
    conf->set("partition.assignment.strategy", "roundrobin", errstr);
    conf->set("event_cb", &reporter_, errstr);

    std::list<std::string>* dump = conf->dump();
    std::cout << "### Global config" << std::endl;
    for (std::list<std::string>::iterator it = dump->begin(); it != dump->end();) {
        std::cout << *it << " = ";
        it++;
        std::cout << *it << std::endl;
        it++;
    }
    std::cout << std::endl;

    errstr.clear();
    consumer_.reset(RdKafka::KafkaConsumer::create(conf, errstr));
    delete conf;
    if (!errstr.empty()) LOGGER_ERROR(std::format("KafkaConsumer: {}", errstr));
    if (consumer_)       LOGGER_INFOR(std::format("Created Kafka consumer '{}'", consumer_->name()));
}

void KafkaConsumer::subscribe_to_user_topic(const std::string& user_id)
{
    if (!consumer_) return;

    std::string user_topic = "user_" + user_id + "_posts";

    std::unique_lock lock(subscribed_topics_mtx_);
    if (subscribed_topics_.find(user_topic) == subscribed_topics_.end()) {
        subscribed_topics_.insert(user_topic);

        // в Kafka подписка работает сразу на набор, т.е. новый набор заменяет существующий,
        // поэтому надо формировать актуальный полный список топиков
        std::vector<std::string> topics(subscribed_topics_.begin(), subscribed_topics_.end());
        consumer_->subscribe(topics);
        LOGGER_DEBUG(std::format("KafkaConsumer: subscribe to topic '{}'", user_topic));
    }
}

void KafkaConsumer::unsubscribe_from_user_topic(const std::string& user_id)
{
    if (!consumer_) return;

    std::string user_topic = "user_" + user_id + "_posts";

    std::unique_lock lock(subscribed_topics_mtx_);
    subscribed_topics_.erase(user_topic);

    // в Kafka подписка работает сразу на набор, т.е. новый набор заменяет существующий,
    // поэтому надо формировать актуальный полный список топиков
    std::vector<std::string> topics(subscribed_topics_.begin(), subscribed_topics_.end());
    consumer_->subscribe(topics);
    LOGGER_DEBUG(std::format("KafkaConsumer: unsubscribe from topic '{}'", user_topic));
}

void KafkaConsumer::subscribe_to_likes_topic(const std::string& user_id)
{
    if (!consumer_) return;

    std::string user_topic = "user_" + user_id + "_post_likes";

    std::unique_lock lock(subscribed_topics_mtx_);
    if (subscribed_topics_.find(user_topic) == subscribed_topics_.end()) {
        subscribed_topics_.insert(user_topic);

        // в Kafka подписка работает сразу на набор, т.е. новый набор заменяет существующий,
        // поэтому надо формировать актуальный полный список топиков
        std::vector<std::string> topics(subscribed_topics_.begin(), subscribed_topics_.end());
        consumer_->subscribe(topics);
        LOGGER_DEBUG(std::format("KafkaConsumer: subscribe to topic '{}'", user_topic));
    }
}

void KafkaConsumer::unsubscribe_from_likes_topic(const std::string& user_id)
{
    if (!consumer_) return;

    std::string user_topic = "user_" + user_id + "_post_likes";

    std::unique_lock lock(subscribed_topics_mtx_);
    subscribed_topics_.erase(user_topic);

    // в Kafka подписка работает сразу на набор, т.е. новый набор заменяет существующий,
    // поэтому надо формировать актуальный полный список топиков
    std::vector<std::string> topics(subscribed_topics_.begin(), subscribed_topics_.end());
    consumer_->subscribe(topics);
    LOGGER_DEBUG(std::format("KafkaConsumer: unsubscribe from topic '{}'", user_topic));
}

void KafkaConsumer::start_consuming()
{
    if (!consumer_) return;
    if (running_) return;

    running_ = true;
    thread_  = std::thread([this]() {
        while (running_) {
            auto message = consumer_->consume(1000);

            switch (message->err()) {
            case RdKafka::ERR__TIMED_OUT:
                break;
            case RdKafka::ERR__PARTITION_EOF:
                LOGGER_DEBUG(std::format("KafkaConsumer thread: EOF reached for all partition(s)"));
                break;
            case RdKafka::ERR__UNKNOWN_TOPIC:
            case RdKafka::ERR__UNKNOWN_PARTITION:
                LOGGER_DEBUG(std::format("KafkaConsumer thread: consume failed, {}", message->errstr()));
                break;
            case RdKafka::ERR_NO_ERROR:
                {
                    msg_cnt_++;
                    msg_bytes_ += message->len();

                    RdKafka::MessageTimestamp ts = message->timestamp();
                    std::string key = "??";
                    if (message->key())
                        key = *message->key();
                    std::string ts_type = "??";
                    if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_CREATE_TIME)
                        ts_type = "create time";
                    else if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_LOG_APPEND_TIME)
                        ts_type = "log append time";
                    LOGGER_DEBUG(std::format("KafkaConsumer thread: read message at offset {}, key: {}, timestamp: {} {}", message->offset(), key, ts_type, ts.timestamp));

                    std::string msg(static_cast<const char*>(message->payload()), message->len());
                    std::string topic = message->topic_name();

                    // извлекаем user_id из названия топика
                    if (topic.find("user_")  == 0) {
                        auto pos = std::string::npos;
                        // "user_<uuid>_posts" -> "<uuid>"
                        if (topic.find("_posts") != std::string::npos) {
                            pos = topic.find("_posts");
                        } else
                        // "user_<uuid>_post_likes" -> "<uuid>"
                        if (topic.find("_post_likes") != std::string::npos) {
                            pos = topic.find("_post_likes");
                        }
                        if (pos != std::string::npos) {
                            std::string user_id = topic.substr(5, pos - 5);
                            if (handler_) {
                                handler_(user_id, msg);
                            }
                        }
                    }
                }
                break;
            default:
                LOGGER_DEBUG(std::format("KafkaConsumer thread: consume failed, {}", message->errstr()));
                break;
            }

            delete message;
        }
    });
}

void KafkaConsumer::stop_consuming()
{
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    if (consumer_) {
        consumer_->close();
    }
}

bool KafkaConsumer::is_message_handler_assigned() const
{
    return handler_assigned_;
}

void KafkaConsumer::assign_message_handler(KafkaConsumer::MessageHandlerFunc handler)
{
    handler_assigned_ = true;
    handler_ = std::move(handler);
}
