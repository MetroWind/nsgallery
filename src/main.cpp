#include <limits>

#include <spdlog/spdlog.h>
#include <Magick++.h>
#include <cxxopts.hpp>

#include "app.hpp"
#include "config.hpp"

constexpr Magick::MagickSizeType BYTES_PER_MIB = 1024 * 1024;

int main([[maybe_unused]] int argc, char** argv)
{
    cxxopts::Options cmd_options("NS Gallery", "Naively simple web gallery");
    cmd_options.add_options()
        ("c,config", "Config file",
         cxxopts::value<std::string>()->default_value("/etc/nsgallery.yaml"))
        ("h,help", "Print this message.");
    auto opts = cmd_options.parse(argc, argv);

    if(opts.count("help"))
    {
        std::cout << cmd_options.help() << std::endl;
        return 0;
    }

    const std::string config_file = opts["config"].as<std::string>();
    const auto config = Configuration::fromYaml(config_file);
    if(!config.has_value())
    {
        std::cerr << "Error: failed to load config." << std::endl;
        return 1;
    }

    Magick::InitializeMagick(*argv);
    if(config->imagemagick_mem_limit_mib > 0)
    {
        constexpr auto MAX_LIMIT_MIB =
            std::numeric_limits<Magick::MagickSizeType>::max() / BYTES_PER_MIB;
        if(config->imagemagick_mem_limit_mib > MAX_LIMIT_MIB)
        {
            std::cerr << "Error: imagemagick-mem-limit-mib is too large."
                      << std::endl;
            return 1;
        }
        Magick::ResourceLimits::memory(
            config->imagemagick_mem_limit_mib * BYTES_PER_MIB);
    }
    // spdlog::set_level(spdlog::level::debug);

    App app(*config);
    app.start();

    return 0;
}
