#include <algorithm>
#include <exception>
#include <filesystem>
#include <mutex>
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
    if(tree["cover"].has_key())
    {
        auto value = tree["cover"].val();
        config.cover = std::string(value.begin(), value.end());
    }
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

const std::vector<std::filesystem::path>*
ItemListCache::get(const std::string& key)
{
    {
        std::shared_lock<std::shared_mutex>(lock);
        auto found = cache.find(key);
        if(found != std::end(cache) && detectStale(key) == CacheStatus::FRESH)
        {
            spdlog::debug("Cache hit on {}.", key);
            return &found->second;
        }
    }
    spdlog::debug("Cache miss on {}.", key);
    std::unique_lock<std::shared_mutex>(lock);
    cache[key] = refresh(key);
    return &cache[key];
}

ImageSource::ImageSource(const Configuration& conf)
        : config(conf), dir(conf.photo_root_dir)
{
    auto stale = [](const std::string& id)
    {
        return CacheStatus::FRESH;
    };

    photo_list_cache.setGetFresh([&](const std::string& album_id)
    {
        std::vector<fs::path> paths;
        auto album_path = dir / album_id;
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
            std::string ext = asciiLower(path.extension().string());
            if(ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".tif"
               || ext == ".tiff" || ext == ".webp" || ext == ".avif")
            {
                auto id = (fs::path(album_id) / fs::path(base_name).stem()).string();
                switch(imageStatus(id))
                {
                case AlbumConfig::EXCLUDE:
                case AlbumConfig::HIDE:
                    continue;
                case AlbumConfig::SHOW:
                    break;
                }
                paths.push_back(fs::path(album_id) / base_name);
                spdlog::debug("{} contains {}.", album_id, paths.back().string());
            }
        }
        return paths;
    });

    album_list_cache.setGetFresh([&](const std::string& album_id)
    {
        std::vector<fs::path> result;
        auto album_path = dir / album_id;
        for(const fs::directory_entry& entry: fs::directory_iterator(album_path))
        {
            if(!(entry.is_directory() || entry.is_symlink()))
            {
                continue;
            }
            const auto& path = entry.path();
            std::string base_name = path.filename().string();
            std::string id = (fs::path(album_id) / base_name).string();
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
    });

    photo_list_cache.setDetectStale(stale);
    album_list_cache.setDetectStale(stale);
}

E<std::vector<ImageFile>> ImageSource::images(const std::string& album)
{
    auto album_path = dir / album;
    AlbumConfig::ItemStatus album_status = albumStatus(album);
    if(album_status == AlbumConfig::EXCLUDE)
    {
        return std::unexpected("Not found.");
    }

    const std::vector<fs::path>& paths = *photo_list_cache.get(album);
    std::vector<ImageFile> result;
    for(auto& path: paths)
    {
        result.emplace_back(dir, (album / path.stem()).string(),
                            dir / path, config);
    }
    return result;
}

E<std::vector<std::string>> ImageSource::albums(const std::string& album)
{
    if(shouldExcludeAlbumFromParent(album))
    {
        return std::unexpected("Not found");
    }

    const auto* paths = album_list_cache.get(album);
    std::vector<std::string> result;
    result.reserve(paths->size());
    std::transform(std::begin(*paths), std::end(*paths), std::back_inserter(result),
                   [](const fs::path& p) { return p.string(); });
    return result;
}

std::optional<ImageFile> ImageSource::image(std::string_view id)
{
    spdlog::debug("Getting image with ID {}...", id);
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
    fs::path path = dir / id;
    AlbumConfig conf = AlbumConfig::fromYamlOrDefault(
        path.parent_path() / ALBUM_CONFIG_FILE);
    AlbumConfig::ItemStatus status =
        conf.getItmeStatus(path.filename().string());
    if(status == AlbumConfig::EXCLUDE)
    {
        return status;
    }
    if(shouldExcludeImageFromParent(id))
    {
        return AlbumConfig::EXCLUDE;
    }
    else
    {
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

std::vector<IDWithName> ImageSource::navChain(std::string_view id) const
{
    if(id.empty())
    {
        return {};
    }
    auto path = fs::path(id);
    auto chain = navChain(path.parent_path().string());
    chain.emplace_back(std::string(id), path.filename().string());
    return chain;
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

std::optional<std::string>
ImageSource::albumCover(const std::string& album_id)
{
    auto album_conf = AlbumConfig::fromYamlOrDefault(
        dir / album_id / ALBUM_CONFIG_FILE);
    if(album_conf.getCover().has_value())
    {
        return (fs::path(album_id) / (*album_conf.getCover())).string();
    }

    auto imgs = images(album_id);
    if(!imgs.has_value() || imgs->empty())
    {
        return std::nullopt;
    }

    return (*imgs)[0].id;
}
