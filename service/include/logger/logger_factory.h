#pragma once

#include "logger/logger_common.h"

namespace SocialNetwork {

namespace Logging {

//
// фабрика, которая может создавать варианты logger, наследованные
// от класса Logger.
// в фабрику можно зарегистрировать свой собственный logger через
// отдельную функцию register_logger(), с указанным именем logger.
// этот logger будет писать в нужное место (например, в syslog()).
// производит экземпляр logger с указанной конфигурацией.
// если в конфигурации будет что-то не так - выбросит исключение
//
class LoggerFactory : public std::unordered_map<std::string, LoggerCreator>
{
public:
    std::shared_ptr<Logger> produce(const LoggerConfig& config) const;
};

LoggerFactory& get_single_factory();

bool register_logger(const std::string_view name, LoggerCreator function_ptr);

} // namespace Logging

} // namespace SocialNetwork
