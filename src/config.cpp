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
    if(tree["listen-address"].has_key())
    {
        auto value = tree["listen-address"].val();
        config.listen_address = std::string(value.begin(), value.end());
    }
    if(tree["listen-port"].has_key())
    {
        if(!getYamlValue(tree["listen-port"], config.listen_port))
        {
            return std::unexpected("Invalid port");
        }
    }
    if(tree["url-prefix"].has_key())
    {
        auto value = tree["url-prefix"].val();
        config.url_prefix = std::string(value.begin(), value.end());
    }
    if(tree["photo-root-dir"].has_key())
    {
        auto value = tree["photo-root-dir"].val();
        config.photo_root_dir = std::string(value.begin(), value.end());
    }
    if(tree["template-dir"].has_key())
    {
        auto value = tree["template-dir"].val();
        std::string path(value.begin(), value.end());
        if(!path.ends_with(std::filesystem::path::preferred_separator))
        {
            path += std::filesystem::path::preferred_separator;
        }
        config.template_dir = std::move(path);
    }
    if(tree["static-dir"].has_key())
    {
        auto value = tree["static-dir"].val();
        config.static_dir = std::string(value.begin(), value.end());
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
    if(tree["exiftool-path"].has_key())
    {
        auto value = tree["exiftool-path"].val();
        config.exiftool_path = std::string(value.begin(), value.end());
    }
    if(tree["imagemagick-mem-limit-mib"].has_key())
    {
        if(!getYamlValue(tree["imagemagick-mem-limit-mib"],
                         config.imagemagick_mem_limit_mib))
        {
            return std::unexpected("Invalid magick mem limit");
        }
    }
    return std::expected<Configuration, std::string>
        {std::in_place, std::move(config)};
}
