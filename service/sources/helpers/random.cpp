#include <cstdlib>
#include <ctime>
#include <float.h>
#include <sys/random.h>
#include "helpers/random.h"

namespace SocialNetwork {

namespace RandomHelpers {

void init_random()
{
    unsigned int seed = 0;
    auto rc = getrandom(reinterpret_cast<void*>(&seed), sizeof(seed), 0);
    if (rc < 0) {
        seed = static_cast<unsigned int>(time(nullptr));
    }
    srand(seed);
}

double get_double()
{
    unsigned long long limit = (1ull << DBL_MANT_DIG) - 1;
    double r = 0.0;
    do {
        r += rand();
        // Assume RAND_MAX is a power-of-2 - 1
        r /= (RAND_MAX/2 + 1)*2.0;
        limit = limit / (RAND_MAX/2 + 1) / 2;
    } while (limit);

    // Use only DBL_MANT_DIG (53) bits of precision.
    if (r < 0.5) {
        volatile double sum = 0.5 + r;
        r = sum - 0.5;
    }
    return r;
}

} // namespace RandomHelpers

} // namespace SocialNetwork
