#include "app.h"

int main(int argc, char **argv)
{
    auto cli_opts = configure_cli_options(argc, argv);
    if (!cli_opts) return EXIT_FAILURE;

    App app(cli_opts);
    app.run();

    return EXIT_SUCCESS;
}
