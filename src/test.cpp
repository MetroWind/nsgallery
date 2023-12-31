#include <spdlog/spdlog.h>
#include <Magick++.h>
#include <cxxopts.hpp>

#include "app.hpp"
#include "config.hpp"

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

    std::string config_file;
    if(opts.count("config"))
    {
        config_file = opts["config"].as<std::string>();
    }
    const auto config = Configuration::fromYaml(config_file);
    if(!config.has_value())
    {
        std::cerr << "Error: failed to load config." << std::endl;
        return 1;
    }

    Magick::InitializeMagick(*argv);
    // spdlog::set_level(spdlog::level::debug);

    App app(*config);
    app.start();

    return 0;
}
