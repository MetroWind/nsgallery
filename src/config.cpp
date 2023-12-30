#include <expected>
#include <optional>
#include <utility>
#include <string_view>

#include <ryml.hpp>
#include <ryml_std.hpp>

#include "config.hpp"
#include "utils.hpp"

std::optional<ImageFormat::Value> ImageFormat::fromStr(std::string s)
{
    std::string ss = asciiLower(std::move(s));
    if(ss == "jpeg")
    {
        return JPEG;
    }
    else if(ss == "webp")
    {
        return WEBP;
    }
    else if(ss == "avif")
    {
        return AVIF;
    }
    else
    {
        return std::nullopt;
    }
}

std::string_view ImageFormat::toExt(ImageFormat::Value v)
{
    switch(v)
    {
    case JPEG:
        return "jpg";
    case WEBP:
        return "webp";
    case AVIF:
        return "avif";
    }
    std::unreachable();
}

std::string_view ImageFormat::contentType(Value v)
{
    switch(v)
    {
    case JPEG:
        return "image/jpeg";
    case WEBP:
        return "image/webp";
    case AVIF:
        return "image/avif";
    }
    std::unreachable();
}

E<Configuration> Configuration::fromYaml(const std::filesystem::path& path)
{
    auto buffer = readFile(path);
    if(!buffer.has_value())
    {
        return std::unexpected(buffer.error());
    }

    ryml::Tree tree = ryml::parse_in_place(ryml::to_substr(*buffer));
    Configuration config;
    if(tree["photo-root-dir"].has_key())
    {
        auto value = tree["photo-root-dir"].val();
        config.photo_root_dir = std::string(value.begin(), value.end());
    }
    if(tree["template-dir"].has_key())
    {
        auto value = tree["template-dir"].val();
        config.template_dir = std::string(value.begin(), value.end());
    }
    if(tree["thumb-size"].has_key())
    {
        if(!getYamlValue(tree["thumb-size"], config.thumb_size))
        {
            return std::unexpected("Invalid thumb size");
        }
    }
    if(tree["thumb-quality"].has_key())
    {
        if(!getYamlValue(tree["thumb-quality"], config.thumb_quality))
        {
            return std::unexpected("Invalid thumb quality");
        }
    }
    if(tree["thumb-format"].has_key())
    {
        auto value = tree["thumb-format"].val();
        std::string s(value.begin(), value.end());
        auto f = ImageFormat::fromStr(std::move(s));
        if(f.has_value())
        {
            config.thumb_format = *f;
        }
    }
    if(tree["present-size"].has_key())
    {
        if(!getYamlValue(tree["present-size"], config.present_size))
        {
            return std::unexpected("Invalid present size");
        }
    }
    if(tree["present-quality"].has_key())
    {
        if(!getYamlValue(tree["present-quality"], config.present_quality))
        {
            return std::unexpected("Invalid present quality");
        }
    }
    if(tree["present-format"].has_key())
    {
        auto value = tree["present-format"].val();
        std::string s(value.begin(), value.end());
        auto f = ImageFormat::fromStr(std::move(s));
        if(f.has_value())
        {
            config.present_format = *f;
        }
    }
    return std::expected<Configuration, std::string>
        {std::in_place, std::move(config)};
}
