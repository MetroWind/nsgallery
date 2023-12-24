#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <utility>

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

std::string AlbumConfig::statusToStr(ItemStatus s)
{
    switch(s)
    {
    case SHOW:
        return "show";
    case HIDE:
        return "hide";
    case EXCLUDE:
        return "exclude";
    }
    std::unreachable();
}

AlbumConfig::ItemStatus
AlbumConfig::getItmeStatus(const std::string& file_base_name)
{
    spdlog::debug("Getting status of {}...", file_base_name);
    spdlog::debug("Excludes {}, hides {}, includes {}.", excludes.size(), hides.size(), includes.size());
    if(excludes.empty())
    {
        if(includes.empty() || includes.contains(file_base_name))
        {
            if(hides.contains(file_base_name))
            {
                spdlog::debug("aaa");
                return AlbumConfig::HIDE;
            }
            else
            {
                spdlog::debug("bbb");
                return AlbumConfig::SHOW;
            }
        }
        else
        {
            spdlog::debug("ccc");
            return AlbumConfig::EXCLUDE;
        }
    }
    else if(excludes.contains(file_base_name))
    {
        spdlog::debug("ddd");
        return AlbumConfig::EXCLUDE;
    }
    else if(hides.contains(file_base_name))
    {
        spdlog::debug("eee");
        return AlbumConfig::HIDE;
    }
    else
    {
        spdlog::debug("fff");
        return AlbumConfig::SHOW;
    }
}

E<std::vector<ImageFile>> ImageSource::images(std::string_view album) const
{
    std::vector<ImageFile> result;
    spdlog::debug("Listing {}...", (dir / album).string());
    auto album_path = dir / album;
    AlbumConfig::ItemStatus album_status = albumStatus(album);
    if(album_status == AlbumConfig::EXCLUDE)
    {
        return std::unexpected("Not found.");
    }
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
        std::string ext = path.extension().string();
        // Convert file extension to lower case. WARNING: This
        // assumes ASCII extension names!
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if(ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".tif"
           || ext == ".tiff" || ext == ".webp" || ext == ".avif")
        {
            auto id = (fs::path(album) / fs::path(base_name).stem()).string();
            switch(imageStatus(id))
            {
            case AlbumConfig::EXCLUDE:
            case AlbumConfig::HIDE:
                continue;
            case AlbumConfig::SHOW:
                break;
            }
            result.emplace_back(dir, id, path, config);
        }
    }
    return result;
}

E<std::vector<std::string>> ImageSource::albums(std::string_view album) const
{
    std::vector<std::string> result;
    auto album_path = dir / album;
    spdlog::debug("Listing {}...", album_path.string());
    if(shouldExcludeAlbumFromParent(album))
    {
        return std::unexpected("Not found");
    }
    for(const fs::directory_entry& entry: fs::directory_iterator(dir / album))
    {
        if(!(entry.is_directory() || entry.is_symlink()))
        {
            continue;
        }
        const auto& path = entry.path();
        std::string base_name = path.filename().string();
        std::string id = (fs::path(album) / base_name).string();
        if(std::move(base_name).starts_with("."))
        {
            continue;
        }
        switch(albumStatus(id))
        {
        case AlbumConfig::EXCLUDE:
        case AlbumConfig::HIDE:
            continue;
        case AlbumConfig::SHOW:
            break;
        }

        result.push_back(std::move(id));
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

AlbumConfig::ItemStatus ImageSource::imageStatus(std::string_view id) const
{
    spdlog::debug("Finding image status of {}...", id);
    fs::path path = dir / id;
    spdlog::debug("Album config is at {}.", (path.parent_path() / ALBUM_CONFIG_FILE).string());
    AlbumConfig conf = AlbumConfig::fromYamlOrDefault(
        path.parent_path() / ALBUM_CONFIG_FILE);
    AlbumConfig::ItemStatus status =
        conf.getItmeStatus(path.filename().string());
    if(status == AlbumConfig::EXCLUDE)
    {
        spdlog::debug("{} is excluded.", id);
        return status;
    }
    if(shouldExcludeImageFromParent(id))
    {
        spdlog::debug("{} is excluded from parent.", id);
        return AlbumConfig::EXCLUDE;
    }
    else
    {
        spdlog::debug("{} is {}.", id, AlbumConfig::statusToStr(status));
        return status;
    }
}

AlbumConfig::ItemStatus ImageSource::albumStatus(std::string_view id) const
{
    if(id.empty())
    {
        return AlbumConfig::SHOW;
    }
    fs::path path = dir / id;
    AlbumConfig conf = AlbumConfig::fromYamlOrDefault(
        path.parent_path() / ALBUM_CONFIG_FILE);
    AlbumConfig::ItemStatus status =
        conf.getItmeStatus(path.filename().string());
    if(status == AlbumConfig::EXCLUDE)
    {
        return status;
    }
    if(shouldExcludeAlbumFromParent(id))
    {
        return AlbumConfig::EXCLUDE;
    }
    else
    {
        return status;
    }
}

bool ImageSource::shouldExcludeImageFromParent(std::string_view id) const
{
    auto parent_id = fs::path(id).parent_path();
    if(parent_id.empty())
    {
        return false;
    }
    AlbumConfig parent_conf = AlbumConfig::fromYamlOrDefault(
        dir / parent_id / ALBUM_CONFIG_FILE);
    if(parent_conf.getItmeStatus(parent_id.filename().string()) ==
       AlbumConfig::EXCLUDE)
    {
        return true;
    }
    else
    {
        return shouldExcludeAlbumFromParent(parent_id.string());
    }
}

bool ImageSource::shouldExcludeAlbumFromParent(std::string_view id) const
{
    if(id.empty())
    {
        return false;
    }
    auto parent_id = fs::path(id).parent_path();
    if(parent_id.empty())
    {
        return false;
    }
    AlbumConfig parent_conf = AlbumConfig::fromYamlOrDefault(
        dir / parent_id / ALBUM_CONFIG_FILE);
    if(parent_conf.getItmeStatus(parent_id.filename().string()) ==
       AlbumConfig::EXCLUDE)
    {
        return true;
    }
    else
    {
        return shouldExcludeAlbumFromParent(parent_id.string());
    }
}
