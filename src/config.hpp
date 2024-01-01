#pragma once

#include <string>
#include <string_view>
#include <optional>

#include <stdint.h>

#include "utils.hpp"

struct ImageFormat
{
public:
    enum Value {JPEG, WEBP, AVIF};
    ImageFormat() = delete;
    static std::optional<Value> fromStr(std::string s);
    static std::string_view toExt(Value v);
    static std::string_view contentType(Value v);
};

class Configuration
{
public:
    std::string listen_address = "localhost";
    uint16_t listen_port = 33565;
    std::string photo_root_dir = ".";
    std::string template_dir = "./templates";
    std::string static_dir = ".";
    uint32_t thumb_size = 128;
    int thumb_quality = 80;
    ImageFormat::Value thumb_format = ImageFormat::AVIF;
    uint32_t present_size = 1280;
    int present_quality = 85;
    ImageFormat::Value present_format = ImageFormat::AVIF;
    std::string exiftool_path = "exiftool";

    static E<Configuration> fromYaml(const std::filesystem::path& path);
};
