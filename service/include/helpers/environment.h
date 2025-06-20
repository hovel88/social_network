#pragma once

#include <string>
#include <optional>

namespace SocialNetwork {

namespace EnvironmentHelpers {

bool has(const std::string& name);
std::optional<std::string> get(const std::string& name);
std::string get(const std::string& name, const std::string& def);

std::string os_name();
std::string os_version();
std::string os_architecture();

// возвращает Ethernet адрес первого найденного интерфейса в формате "xx:xx:xx:xx:xx:xx"
std::optional<std::string> node_id();
std::string node_name();

int processor_count();

} // namespace EnvironmentHelpers

} // namespace SocialNetwork
