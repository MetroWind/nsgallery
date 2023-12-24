#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

#include <spdlog/spdlog.h>

#include "config.hpp"
#include "image.hpp"
#include "image_source.hpp"
#include "utils.hpp"

namespace fs = std::filesystem;

AlbumConfig AlbumConfig::fromYamlOrDefault(const fs::path& file)
{
    auto buffer = readFile(file);
    if(!buffer.has_value())
    {
        return AlbumConfig{};
    }

    ryml::Tree tree = ryml::parse_in_place(ryml::to_substr(*buffer));
    AlbumConfig config;
    if(tree["hides"].has_key())
    {
        for(const auto& item: tree["hides"])
        {
            auto value = item.val();
            config.hides.emplace(value.begin(), value.end());
        }
    }
    if(tree["excludes"].has_key())
    {
        for(const auto& item: tree["excludes"])
        {
            auto value = item.val();
            config.excludes.emplace(value.begin(), value.end());
        }
    }
    if(tree["includes"].has_key())
    {
        for(const auto& item: tree["includes"])
        {
            auto value = item.val();
            config.includes.emplace(value.begin(), value.end());
        }
    }
    return config;
}

AlbumConfig::ItemStatus
AlbumConfig::getItmeStatus(const std::string& file_base_name)
{
    if(excludes.empty())
    {
        if(includes.empty() || includes.contains(file_base_name))
        {
            if(hides.contains(file_base_name))
            {
                return AlbumConfig::HIDE;
            }
            else
            {
                return AlbumConfig::SHOW;
            }
        }
        else
        {
            return AlbumConfig::EXCLUDE;
        }
    }
    else if(excludes.contains(file_base_name))
    {
        return AlbumConfig::EXCLUDE;
    }
    else if(hides.contains(file_base_name))
    {
        return AlbumConfig::HIDE;
    }
    else
    {
        return AlbumConfig::SHOW;
    }
}

E<std::vector<ImageFile>> ImageSource::images(std::string_view album) const
{
    std::vector<ImageFile> result;
    spdlog::debug("Listing {}...", (dir / album).string());
    auto album_path = dir / album;
    if(!album.empty())
    {
        AlbumConfig parent_conf = AlbumConfig::fromYamlOrDefault(
            album_path.parent_path() / ALBUM_CONFIG_FILE);
        if(parent_conf.getItmeStatus(album_path.filename().string()) ==
           AlbumConfig::EXCLUDE)
        {
            return std::unexpected("Not found");
        }
    }

    AlbumConfig album_conf =
        AlbumConfig::fromYamlOrDefault(album_path / ALBUM_CONFIG_FILE);
    for(const fs::directory_entry& entry: fs::directory_iterator(album_path))
    {
        if(!(entry.is_regular_file() || entry.is_symlink()))
        {
            continue;
        }
        const auto& path = entry.path();
        const std::string base_name = path.filename().string();
        if(base_name.starts_with("."))
        {
            continue;
        }

        switch(album_conf.getItmeStatus(base_name))
        {
        case AlbumConfig::EXCLUDE:
        case AlbumConfig::HIDE:
            continue;
        case AlbumConfig::SHOW:
            break;
        }

        std::string ext = path.extension().string();
        // Convert file extension to lower case. WARNING: This
        // assumes ASCII extension names!
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if(ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".tif"
           || ext == ".tiff" || ext == ".webp" || ext == ".avif")
        {
            auto base = path.filename().stem();
            auto id = (fs::path(album) / base).string();
            result.emplace_back(dir, id, entry.path(), config);
        }
    }
    return result;
}

E<std::vector<std::string>> ImageSource::albums(std::string_view album) const
{
    std::vector<std::string> result;
    auto album_path = dir / album;
    spdlog::debug("Listing {}...", album_path.string());
    if(!album.empty())
    {
        AlbumConfig parent_conf = AlbumConfig::fromYamlOrDefault(
            album_path.parent_path() / ALBUM_CONFIG_FILE);
        if(parent_conf.getItmeStatus(album_path.filename().string()) ==
           AlbumConfig::EXCLUDE)
        {
            return std::unexpected("Not found");
        }
    }
    AlbumConfig album_conf =
        AlbumConfig::fromYamlOrDefault(album_path / ALBUM_CONFIG_FILE);
    for(const fs::directory_entry& entry: fs::directory_iterator(dir / album))
    {
        if(!(entry.is_directory() || entry.is_symlink()))
        {
            continue;
        }
        const auto& path = entry.path();
        std::string base_name = path.filename().string();
        if(base_name.starts_with("."))
        {
            continue;
        }
        switch(album_conf.getItmeStatus(base_name))
        {
        case AlbumConfig::EXCLUDE:
        case AlbumConfig::HIDE:
            continue;
        case AlbumConfig::SHOW:
            break;
        }

        result.push_back((fs::path(album) / std::move(base_name)).string());
    }
    return result;
}

std::optional<ImageFile> ImageSource::image(std::string_view id) const
{
    auto album_path = fs::path(id).parent_path();
    auto imgs = images(album_path.string());
    if(!imgs.has_value())
    {
        return std::nullopt;
    }
    auto found = std::find_if(std::begin(*imgs), std::end(*imgs),
                              [id](const auto& img) { return img.id == id; });
    if(found == std::end(*imgs))
    {
        return std::nullopt;
    }
    return *found;
}
