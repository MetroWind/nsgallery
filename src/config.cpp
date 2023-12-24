#include <expected>
#include <ryml.hpp>
#include <ryml_std.hpp>

#include "config.hpp"
#include "utils.hpp"

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

    return std::expected<Configuration, std::string>
        {std::in_place, std::move(config)};
}
