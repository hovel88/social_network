#pragma once

#include <string>

namespace SocialNetwork {

namespace ThreadHelpers {

void block_signals();

void set_name(pthread_t th, const std::string& name);
std::string get_name(pthread_t th);

} // namespace ThreadHelpers

} // namespace SocialNetwork
