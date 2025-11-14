#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <shared_mutex>

#include "database.h"
#include "kafka_likes_producer.h"
#include "grpc_likes_client.h"

class LikesSaga
{
public:
    enum class Status {
        PENDING,
        IN_PROGRESS,
        COMPLETED,
        COMPENSATING,
        FAILED
    };

    struct Step {
        std::function<bool(const std::string& saga_id, const std::string& user_id, const std::string& post_id)> execute{};
        std::function<bool(const std::string& saga_id, const std::string& user_id, const std::string& post_id)> compensate{};
        std::string name{};
        bool        completed{false};
    };

public:
    ~LikesSaga() = default;
    LikesSaga(const LikesSaga&) = default;
    LikesSaga(LikesSaga&&) = default;
    LikesSaga& operator=(const LikesSaga&) = default;
    LikesSaga& operator=(LikesSaga&&) = default;

    static LikesSaga& instance()
    {
        static LikesSaga singleton;
        return singleton;
    }

    // запускает сценарий SAGA и возвращает saga_id, чтобы потом проверить результат
    std::string add_like(const std::string& user_id, const std::string& post_id);
    std::string del_like(const std::string& user_id, const std::string& post_id);

    Status      get_status(const std::string& saga_id) const;
    std::string get_status_message(const std::string& saga_id) const;
    int32_t     get_result(const std::string& saga_id) const;

private:
    LikesSaga()
    :   logger_(Configuration::instance().get_logger()),
        db_(Database::instance()),
        grpc_client_(GrpcLikesClient::instance()),
        kafka_producer_(KafkaLikesProducer::instance())
    {}

    void update_status(const std::string& saga_id, Status status, const std::string& message = "");

    bool execute_saga(const std::string& saga_id, const std::string& user_id, const std::string& post_id, const std::vector<Step>& steps);
    bool compensate_saga(const std::string& saga_id, const std::string& user_id, const std::string& post_id, const std::vector<Step>& steps, int failed_step_index);

    // шаги SAGA для изменения лайка
    bool step1_save_inc_like(const std::string& saga_id, const std::string& user_id, const std::string& post_id);
    bool step1_save_inc_like_compensate(const std::string& saga_id, const std::string& user_id, const std::string& post_id);

    bool step1_save_dec_like(const std::string& saga_id, const std::string& user_id, const std::string& post_id);
    bool step1_save_dec_like_compensate(const std::string& saga_id, const std::string& user_id, const std::string& post_id);

    bool step2_inc_likes_count(const std::string& saga_id, const std::string& user_id, const std::string& post_id);
    bool step2_inc_likes_count_compensate(const std::string& saga_id, const std::string& user_id, const std::string& post_id);

    bool step2_dec_likes_count(const std::string& saga_id, const std::string& user_id, const std::string& post_id);
    bool step2_dec_likes_count_compensate(const std::string& saga_id, const std::string& user_id, const std::string& post_id);

    bool step3_get_author_friends(const std::string& saga_id, const std::string& user_id, const std::string& post_id);
    bool step3_get_author_friends_compensate(const std::string& saga_id, const std::string& user_id, const std::string& post_id);

    bool step4_notify_friends(const std::string& saga_id, const std::string& user_id, const std::string& post_id);
    bool step4_notify_friends_compensate(const std::string& saga_id, const std::string& user_id, const std::string& post_id);

private:
    std::shared_ptr<Logging::Logger>    logger_{nullptr};
    Database&                           db_;
    GrpcLikesClient&                    grpc_client_;
    KafkaLikesProducer&                 kafka_producer_;

    struct Context {
        int32_t                     likes_count{0};
        std::vector<std::string>    friends_ids{};
    };

    mutable std::shared_mutex                           mutex_{};
    std::unordered_map<std::string, std::vector<Step>>  saga_steps_{};
    std::unordered_map<std::string, Context>            saga_context_{};
    std::unordered_map<std::string, Status>             saga_status_{};
    std::unordered_map<std::string, std::string>        saga_status_messages_{};
};
