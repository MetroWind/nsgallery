#pragma once

#include <memory>
#include <string>
#include <optional>

#include "utils.hpp"

struct ImageFormat
{
public:
    enum Value {JPEG, WEBP, AVIF};
    ImageFormat() = delete;
    static std::optional<Value> fromStr(std::string s);
    static std::string toExt(Value v);
    static std::string contentType(Value v);
};

class Configuration
{
public:
    std::string photo_root_dir = ".";
    std::string template_dir = "./templates";
    std::string static_dir = ".";
    uint32_t thumb_size = 128;
    int thumb_quality = 80;
    ImageFormat::Value thumb_format = ImageFormat::AVIF;
    uint32_t present_size = 1280;
    int present_quality = 85;
    ImageFormat::Value present_format = ImageFormat::AVIF;

    static E<Configuration> fromYaml(const std::filesystem::path& path);
};
