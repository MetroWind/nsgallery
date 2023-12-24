#include <spdlog/spdlog.h>
#include <Magick++.h>

#include "app.hpp"
#include "config.hpp"

int main(int argc, char** argv)
{
    Magick::InitializeMagick(*argv);
    spdlog::set_level(spdlog::level::debug);

    const Configuration config {
        .photo_root_dir = "/mnt/stuff/Pictures",
        .template_dir = "../templates/",
    };
    App app(config);
    app.start();

    return 0;

}
