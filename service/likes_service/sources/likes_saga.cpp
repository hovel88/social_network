#include "likes_saga.h"

#include <nlohmann/json.hpp>
#include <format>

#include "configuration.h"
#include "logger/logger.h"
#include "helpers/uuid.h"

LikesSaga::Status LikesSaga::get_status(const std::string& saga_id) const
{
    std::shared_lock lock(mutex_);
    auto it = saga_status_.find(saga_id);
    return it != saga_status_.end() ? it->second : Status::FAILED;
}

std::string LikesSaga::get_status_message(const std::string& saga_id) const
{
    std::shared_lock lock(mutex_);
    auto it = saga_status_messages_.find(saga_id);
    return it != saga_status_messages_.end() ? it->second : "SAGA not found";
}

int32_t LikesSaga::get_result(const std::string& saga_id) const
{
    std::shared_lock lock(mutex_);
    auto it = saga_context_.find(saga_id);
    return it != saga_context_.end() ? it->second.likes_count : 0;
}

void LikesSaga::update_status(const std::string& saga_id, Status status, const std::string& message)
{
    std::unique_lock lock(mutex_);
    saga_status_[saga_id] = status;
    saga_status_messages_[saga_id] = message;
}

bool LikesSaga::execute_saga(const std::string& saga_id, const std::string& user_id, const std::string& post_id, const std::vector<Step>& steps)
{
    update_status(saga_id, Status::PENDING, "SAGA started");
    LOGGER_INFOR(std::format("SAGA '{}': starting execution for user '{}', post '{}'", saga_id, user_id, post_id));

    for (size_t i = 0; i < steps.size(); ++i) {
        auto& step = steps[i];
        LOGGER_INFOR(std::format("SAGA '{}': executing step {} - {}", saga_id, (i + 1), step.name));

        bool step_success = step.execute(saga_id, user_id, post_id);
        if (!step_success) {
            LOGGER_ERROR(std::format("SAGA '{}': step {} failed, starting compensation", saga_id, (i + 1)));
            compensate_saga(saga_id, user_id, post_id, steps, i);
            return false;
        } else {
            std::unique_lock lock(mutex_);
            if (saga_steps_.find(saga_id) != saga_steps_.end()) {
                saga_steps_[saga_id][i].completed = true;
            }
        }
    }

    update_status(saga_id, Status::COMPLETED, "All SAGA steps completed successfully");
    return true;
}

bool LikesSaga::compensate_saga(const std::string& saga_id, const std::string& user_id, const std::string& post_id, const std::vector<Step>& steps, int failed_step_index)
{
    update_status(saga_id, Status::COMPENSATING, "compensation started");
    LOGGER_INFOR(std::format("SAGA '{}': starting compensation from step {}", saga_id, failed_step_index));

    bool all_compensations_successful = true;
    for (int i = failed_step_index; i >= 0; --i) {
        auto& step = steps[i];
        // Компенсируем только выполненные шаги
        bool step_completed = false;
        {
            std::shared_lock lock(mutex_);
            if (saga_steps_.find(saga_id) != saga_steps_.end()
            &&  static_cast<size_t>(i) < saga_steps_[saga_id].size()) {
                step_completed = saga_steps_[saga_id][i].completed;
            }
        }

        if (step_completed) {
            LOGGER_INFOR(std::format("SAGA '{}': compensating step {} - ", saga_id, (i + 1), step.name));
            bool compensation_success = step.compensate(saga_id, user_id, post_id);
            if (!compensation_success) {
                LOGGER_ERROR(std::format("SAGA '{}': compensation for step {} failed", saga_id, (i + 1)));
                all_compensations_successful = false;
            } else {
                LOGGER_INFOR(std::format("SAGA '{}': step {} compensated successfully", saga_id, (i + 1)));
            }
        }
    }

    if (all_compensations_successful) {
        update_status(saga_id, Status::FAILED, "SAGA failed but all compensations completed");
    }

    return all_compensations_successful;
}

std::string LikesSaga::add_like(const std::string& user_id, const std::string& post_id)
{
    // шаги SAGA для добавления лайка
    static const std::vector<Step> steps = {
        {
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step1_save_inc_like(saga_id, user_id, post_id);
            },
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step1_save_inc_like_compensate(saga_id, user_id, post_id);
            },
            "SAVE_LIKE_INFO"
        },
        {
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step2_inc_likes_count(saga_id, user_id, post_id);
            },
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step2_inc_likes_count_compensate(saga_id, user_id, post_id);
            },
            "INCREMENT_LIKE_COUNT"
        },
        {
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step3_get_author_friends(saga_id, user_id, post_id);
            },
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step3_get_author_friends_compensate(saga_id, user_id, post_id);
            },
            "GET_AUTHOR_FRIENDS"
        },
        {
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step4_notify_friends(saga_id, user_id, post_id);
            },
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step4_notify_friends_compensate(saga_id, user_id, post_id);
            },
            "NOTIFY_FRIENDS"
        }
    };

    std::string saga_id = std::string("like_saga_") + UuidHelpers::Uuid::random_based().to_string_lowercase();
    {
        std::unique_lock lock(mutex_);
        saga_steps_[saga_id] = steps;
    }

    // запускаем выполнение SAGA
    bool success = execute_saga(saga_id, user_id, post_id, steps);
    if (!success) {
        LOGGER_WARNG(std::format("SAGA '{}' failed during execution", saga_id));
    }
    return saga_id;
}

std::string LikesSaga::del_like(const std::string& user_id, const std::string& post_id)
{
    // шаги SAGA для удаления лайка
    static const std::vector<Step> steps = {
        {
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step1_save_dec_like(saga_id, user_id, post_id);
            },
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step1_save_dec_like_compensate(saga_id, user_id, post_id);
            },
            "SAVE_LIKE_INFO"
        },
        {
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step2_dec_likes_count(saga_id, user_id, post_id);
            },
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step2_dec_likes_count_compensate(saga_id, user_id, post_id);
            },
            "DECREMENT_LIKE_COUNT"
        },
        {
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step3_get_author_friends(saga_id, user_id, post_id);
            },
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step3_get_author_friends_compensate(saga_id, user_id, post_id);
            },
            "GET_AUTHOR_FRIENDS"
        },
        {
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step4_notify_friends(saga_id, user_id, post_id);
            },
            [this](const std::string& saga_id, const std::string& user_id, const std::string& post_id) {
                return step4_notify_friends_compensate(saga_id, user_id, post_id);
            },
            "NOTIFY_FRIENDS"
        }
    };

    std::string saga_id = std::string("like_saga_") + UuidHelpers::Uuid::random_based().to_string_lowercase();
    {
        std::unique_lock lock(mutex_);
        saga_steps_[saga_id] = steps;
    }

    // запускаем выполнение SAGA
    bool success = execute_saga(saga_id, user_id, post_id, steps);
    if (!success) {
        LOGGER_WARNG(std::format("SAGA '{}' failed during execution", saga_id));
    }
    return saga_id;
}

bool LikesSaga::step1_save_inc_like(const std::string& saga_id, const std::string& user_id, const std::string& post_id)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': step 1 - recording like in local DB", saga_id));
        auto rv = db_.update_like(user_id, post_id, saga_id, "INCREMENT");
        if (!rv.error_str.empty()) {
            LOGGER_DEBUG(std::format("SAGA '{}': step 1 failed - {}", saga_id, rv.error_str));
            update_status(saga_id, Status::FAILED, "step 1 failed");
        } else {
            LOGGER_DEBUG(std::format("SAGA '{}': step 1 completed successfully", saga_id));
            update_status(saga_id, Status::IN_PROGRESS, "like recorded in local database");
            return true;
        }
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 1 failed with exception: {}", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in step 1: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step1_save_inc_like_compensate(const std::string& saga_id, const std::string& /*user_id*/, const std::string& /*post_id*/)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': compensating step 1 - removing like record (INCREMENT)", saga_id));
        auto rv = db_.remove_like_by_saga(saga_id);
        if (!rv.error_str.empty()) {
            LOGGER_DEBUG(std::format("SAGA '{}': step 1 compensation failed - record not found", saga_id));
            update_status(saga_id, Status::FAILED, "step 1 compensation failed");
        } else {
            LOGGER_DEBUG(std::format("SAGA '{}': step 1 compensation completed successfully", saga_id));
            return true;
        }
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 1 compensation failed with exception: {}", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in compensation step 1: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step1_save_dec_like(const std::string& saga_id, const std::string& user_id, const std::string& post_id)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': step 1 - recording like (DECREMENT) in local DB", saga_id));
        auto rv = db_.update_like(user_id, post_id, saga_id, "DECREMENT");
        if (!rv.error_str.empty()) {
            LOGGER_DEBUG(std::format("SAGA '{}': step 1 failed - {}", saga_id, rv.error_str));
            update_status(saga_id, Status::FAILED, "step 1 failed");
        } else {
            LOGGER_DEBUG(std::format("SAGA '{}': step 1 completed successfully", saga_id));
            update_status(saga_id, Status::IN_PROGRESS, "like recorded in local database");
            return true;
        }
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 1 failed with exception: {}", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in step 1: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step1_save_dec_like_compensate(const std::string& saga_id, const std::string& /*user_id*/, const std::string& /*post_id*/)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': compensating step 1 - removing like record (DECREMENT)", saga_id));
        auto rv = db_.remove_like_by_saga(saga_id);
        if (!rv.error_str.empty()) {
            LOGGER_DEBUG(std::format("SAGA '{}': step 1 compensation failed - record not found", saga_id));
            update_status(saga_id, Status::FAILED, "step 1 compensation failed");
        } else {
            LOGGER_DEBUG(std::format("SAGA '{}': step 1 compensation completed successfully", saga_id));
            return true;
        }
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 1 compensation failed with exception: {}", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in compensation step 1: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step2_inc_likes_count(const std::string& saga_id, const std::string& /*user_id*/, const std::string& post_id)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': step 2 - incrementing like count in social_service", saga_id));

        auto rv = grpc_client_.increment_likes(post_id, saga_id);
        if (!rv.success || !rv.error_str.empty()) {
            LOGGER_DEBUG(std::format("SAGA '{}': step 2 failed - {}", saga_id, rv.error_str));
            update_status(saga_id, Status::FAILED, "step 2 failed");
        } else {
            {
                std::unique_lock lock(mutex_);
                saga_context_[saga_id].likes_count = rv.likes_amount;
            }
            LOGGER_DEBUG(std::format("SAGA '{}': step 2 completed successfully, new count {}", saga_id, rv.likes_amount));
            update_status(saga_id, Status::IN_PROGRESS, "like count incremented in social_service");
            return true;
        }
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 2 failed with exception: {}", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in step 2: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step2_inc_likes_count_compensate(const std::string& saga_id, const std::string& /*user_id*/, const std::string& post_id)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': compensating step 2 - decrementing like count", saga_id));

        auto rv = grpc_client_.decrement_likes(post_id, saga_id);
        if (!rv.success || !rv.error_str.empty()) {
            LOGGER_DEBUG(std::format("SAGA '{}': step 2 compensation failed - {}", saga_id, rv.error_str));
            update_status(saga_id, Status::FAILED, "step 2 compensation failed");
        } else {
            LOGGER_DEBUG(std::format("SAGA '{}': step 2 compensation completed successfully", saga_id));
            return true;
        }
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 2 compensation failed with exception: ", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in compensation step 2: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step2_dec_likes_count(const std::string& saga_id, const std::string& /*user_id*/, const std::string& post_id)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': step 2 - decrementing like count in social_service", saga_id));

        auto rv = grpc_client_.decrement_likes(post_id, saga_id);
        if (!rv.success || !rv.error_str.empty()) {
            LOGGER_DEBUG(std::format("SAGA '{}': step 2 failed - {}", saga_id, rv.error_str));
            update_status(saga_id, Status::FAILED, "step 2 failed");
        } else {
            {
                std::unique_lock lock(mutex_);
                saga_context_[saga_id].likes_count = rv.likes_amount;
            }
            LOGGER_DEBUG(std::format("SAGA '{}': step 2 completed successfully, new count {}", saga_id, rv.likes_amount));
            update_status(saga_id, Status::IN_PROGRESS, "like count incremented in social_service");
            return true;
        }
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 2 failed with exception: {}", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in step 2: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step2_dec_likes_count_compensate(const std::string& saga_id, const std::string& /*user_id*/, const std::string& post_id)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': compensating step 2 - incrementing like count", saga_id));

        auto rv = grpc_client_.increment_likes(post_id, saga_id);
        if (!rv.success || !rv.error_str.empty()) {
            LOGGER_DEBUG(std::format("SAGA '{}': step 2 compensation failed - {}", saga_id, rv.error_str));
            update_status(saga_id, Status::FAILED, "step 2 compensation failed");
        } else {
            LOGGER_DEBUG(std::format("SAGA '{}': step 2 compensation completed successfully", saga_id));
            return true;
        }
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 2 compensation failed with exception: ", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in compensation step 2: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step3_get_author_friends(const std::string& saga_id, const std::string& /*user_id*/, const std::string& post_id)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': step 3 - getting author's friends", saga_id));

        auto rv = grpc_client_.get_author_friends(post_id, saga_id);
        if (!rv.success || !rv.error_str.empty()) {
            LOGGER_DEBUG(std::format("SAGA '{}': step 3 failed - {}", saga_id, rv.error_str));
            update_status(saga_id, Status::FAILED, "step 3 failed");
        } else {
            {
                std::unique_lock lock(mutex_);
                saga_context_[saga_id].friends_ids = rv.friend_ids;
            }
            LOGGER_DEBUG(std::format("SAGA '{}': step 3 completed successfully", saga_id));
            update_status(saga_id, Status::IN_PROGRESS, "author friends retrieved");
            return true;
        }
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 3 failed with exception: ", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in Step 3: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step3_get_author_friends_compensate(const std::string& saga_id, const std::string& /*user_id*/, const std::string& /*post_id*/)
{
    LOGGER_DEBUG(std::format("SAGA '{}': no compensation needed for step 3 (read-only operation)", saga_id));
    return true;
}

bool LikesSaga::step4_notify_friends(const std::string& saga_id, const std::string& /*user_id*/, const std::string& post_id)
{
    try {
        LOGGER_DEBUG(std::format("SAGA '{}': step 4 - notifying friends via Kafka", saga_id));
        Context context;
        {
            std::unique_lock lock(mutex_);
            if (saga_context_.find(saga_id) != saga_context_.end()) {
                context = saga_context_[saga_id];
            }
        }

        nlohmann::json msg{};
        msg =   {
                {"saga_id",     saga_id},
                {"post_id",     post_id},
                {"likes_count", context.likes_count},
                {"timestamp",   std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())}
                };
        kafka_producer_.produce_to_user_topics(context.friends_ids, msg.dump());

        LOGGER_DEBUG(std::format("SAGA '{}': step 4 completed successfully", saga_id));
        LOGGER_INFOR(std::format("SAGA '{}': COMPLETED SUCCESSFULLY", saga_id));
        update_status(saga_id, Status::COMPLETED, "like added successfully and friends notified");
        return true;
    } catch (std::exception& ex) {
        LOGGER_DEBUG(std::format("SAGA '{}': step 4 failed with exception: ", saga_id, ex.what()));
        update_status(saga_id, Status::FAILED, "exception in step 4: " + std::string(ex.what()));
    }
    return false;
}

bool LikesSaga::step4_notify_friends_compensate(const std::string& saga_id, const std::string& /*user_id*/, const std::string& /*post_id*/)
{
    LOGGER_DEBUG(std::format("SAGA '{}': no compensation needed for step 4 (idempotent operation)", saga_id));
    return true;
}
