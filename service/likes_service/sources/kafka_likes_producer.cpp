#include "kafka_likes_producer.h"

#include <format>

#include "helpers/environment.h"

KafkaLikesProducer::KafkaDeliveryReporter::KafkaDeliveryReporter()
:   logger_(Configuration::instance().get_logger())
{
}

void KafkaLikesProducer::KafkaDeliveryReporter::dr_cb(RdKafka::Message& message)
{
    // If message.err() is non-zero the message delivery failed permanently for the message
    if (message.err()) {
        LOGGER_ERROR(std::format("KafkaDeliveryReporter: message delivery failed. {}", message.errstr()));
    } else {
        LOGGER_DEBUG(std::format("KafkaDeliveryReporter: message delivered to topic '{}' [{}] at offset {}", message.topic_name(),  message.partition(), message.offset()));
    }
}

// --------------------------------------------------------

KafkaLikesProducer::~KafkaLikesProducer()
{
    if (producer_) {
        LOGGER_DEBUG(std::format("KafkaLikesProducer flush: flushing final messages..."));
        producer_->flush(1 * 1000); // wait for max 1 second
        if (producer_->outq_len() > 0) {
            LOGGER_ERROR(std::format("KafkaLikesProducer flush: {} message(s) were not delivered", producer_->outq_len()));
        }
    }
}

void KafkaLikesProducer::initialize(const std::string& brokers)
{
    std::string errstr;
    auto conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

    conf->set("bootstrap.servers", brokers, errstr); // адреса брокеров Kafka
    conf->set("client.id", EnvironmentHelpers::node_name(), errstr);
    conf->set("message.timeout.ms", "5000", errstr); // таймаут отправки
    conf->set("queue.buffering.max.messages", "100000", errstr);
    conf->set("compression.type", "snappy", errstr); // сжатие
    conf->set("retries", "5", errstr);
    // conf->set("acks", "all", errstr); // ждём подтверждения от всех реплик
    // conf->set("max.in.flight.requests.per.connection", "1", errstr);
    conf->set("dr_cb", &reporter_, errstr);

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
    producer_.reset(RdKafka::Producer::create(conf, errstr));
    delete conf;
    if (!errstr.empty()) LOGGER_ERROR(std::format("KafkaLikesProducer: {}", errstr));
    if (producer_)       LOGGER_INFOR(std::format("Created Kafka producer {}", producer_->name()));
}

void KafkaLikesProducer::produce(const std::string& topic, const std::string& message)
{
    if (!producer_) return;
    auto err = producer_->produce(topic,
                    RdKafka::Topic::PARTITION_UA,   // Unassigned partition. The builtin partitioner will be used to assign the message to a topic based on the message key, or random partition if the key is not set
                    RdKafka::Producer::RK_MSG_COPY, // Make a copy of the value
                    const_cast<char*>(message.data()), message.size(),
                    /*key=*/nullptr, /*key_len=*/0, // Key
                    /*timestamp=*/0,                // Timestamp (defaults to current time)
                    /*msg_opaque=*/nullptr);        // Per-message opaque value passed to delivery report
    if (err != RdKafka::ERR_NO_ERROR) {
        LOGGER_ERROR(std::format("KafkaLikesProducer produce: failed to produce to topic '{}': {}", topic, RdKafka::err2str(err)));
    } else {
        LOGGER_DEBUG(std::format("KafkaLikesProducer produce: enqueued message ({} byte(s)) for topic '{}'", message.size(), topic));
    }
    producer_->poll(0);
}

void KafkaLikesProducer::produce_to_user_topics(const std::vector<std::string>& friend_ids,
                                                const std::string& message)
{
    for (const auto& friend_id : friend_ids) {
        std::string topic = "user_" + friend_id + "_post_likes";
        produce(topic, message);
    }
}
