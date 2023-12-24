#pragma once

#include <string>

#include "utils.hpp"

class Configuration
{
public:
    std::string photo_root_dir = ".";
    std::string template_dir = "./templates";
    uint32_t thumb_size = 128;
    int thumb_quality = 80;
    uint32_t present_size = 1280;
    int present_quality = 85;

    static E<Configuration> fromYaml(const std::filesystem::path& path);
};
