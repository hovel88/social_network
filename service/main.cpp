#include "app.h"

int main(int argc, char **argv)
{
    auto cli_opts = SocialNetwork::configure_cli_options(argc, argv);
    if (!cli_opts) return EXIT_FAILURE;

    SocialNetwork::App app(cli_opts);
    app.run();

    return EXIT_SUCCESS;
}
