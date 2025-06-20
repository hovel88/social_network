#include "logger/logger.h"
#include "logger/logger_factory.h"

namespace SocialNetwork {

namespace Logging {

extern bool null_logger_registered;
extern bool stdout_logger_registered;
extern bool stderr_logger_registered;
extern bool file_logger_registered;

static void registration_()
{
    if (null_logger_registered)   std::cout << "null logger registered" << std::endl;
    if (stdout_logger_registered) std::cout << "stdout logger registered" << std::endl;
    if (stderr_logger_registered) std::cout << "stderr logger registered" << std::endl;
    if (file_logger_registered)   std::cout << "file logger registered" << std::endl;
}

} // namespace Logging



// --------------------------------------------------------

std::shared_ptr<Logging::Logger> Logging::configure_logger(const LoggerConfig& config)
{
    static std::once_flag flag;
    std::call_once(flag, registration_);
    return get_single_factory().produce(config);
}

} // namespace SocialNetwork
