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

const IDWithPath& ItemListCache::get(const std::string& key)
{
    {
        std::shared_lock<std::shared_mutex> l(lock);
        auto found = cache.find(key);
        if(found != std::end(cache) && detectStale(key) == CacheStatus::FRESH)
        {
            spdlog::debug("Cache hit on {}.", key);
            return found->second;
        }
    }
    spdlog::debug("Cache miss on {}.", key);
    std::unique_lock<std::shared_mutex> l(lock);
    cache[key] = refresh(key);
    return cache[key];
}

std::vector<std::reference_wrapper<const std::string>>
orderedIDsFromIDWithPath(const IDWithPath& map)
{
    std::vector<std::reference_wrapper<const std::string>> ids;
    ids.reserve(map.size());
    for(const auto& id_path: map)
    {
        ids.emplace_back(id_path.first);
    }
    std::sort(std::begin(ids), std::end(ids), [](auto ref1, auto ref2)
    {
        return ref1.get() < ref2.get();
    });
    return ids;
}

ImageSource::ImageSource(const Configuration& conf)
        : config(conf), dir(conf.photo_root_dir),
          thumb_manager(Representation::THUMB, conf),
          present_manager(Representation::PRESENT, conf)
{
    auto stale = []([[maybe_unused]] const std::string& id)
    {
        return CacheStatus::FRESH;
    };

    photo_list_cache.setGetFresh([&](const std::string& album_id)
    {
        IDWithPath paths;
        auto album_path = dir / album_id;
        for(const fs::directory_entry& entry:
                fs::directory_iterator(album_path))
        {
            if(!(entry.is_regular_file() || entry.is_symlink()))
            {
                continue;
            }
            auto& path = entry.path();
            const std::string base_name = path.filename().string();
            if(base_name.starts_with("."))
            {
                continue;
            }
            std::string ext = asciiLower(path.extension().string());
            if(ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".tif"
               || ext == ".tiff" || ext == ".webp" || ext == ".avif")
            {
                auto id = (fs::path(album_id) / fs::path(base_name).stem())
                    .string();
                switch(imageStatus(id))
                {
                case AlbumConfig::EXCLUDE:
                case AlbumConfig::HIDE:
                    continue;
                case AlbumConfig::SHOW:
                    break;
                }
                paths.emplace(id, std::move(path));
                spdlog::debug("{} contains {}.", album_id, std::move(id));
            }
        }
        return paths;
    });

    album_list_cache.setGetFresh([&](const std::string& album_id)
    {
        IDWithPath result;
        auto album_path = dir / album_id;
        for(const fs::directory_entry& entry:
                fs::directory_iterator(album_path))
        {
            if(!(entry.is_directory() || entry.is_symlink()))
            {
                continue;
            }
            auto& path = entry.path();
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

            result.emplace(std::move(id), std::move(path));
        }
        return result;
    });

    photo_list_cache.setDetectStale(stale);
    album_list_cache.setDetectStale(stale);
}

E<IDWithPathRef> ImageSource::images(const std::string& album)
{
    auto album_path = dir / album;
    AlbumConfig::ItemStatus album_status = albumStatus(album);
    if(album_status == AlbumConfig::EXCLUDE)
    {
        return std::unexpected("Not found.");
    }

    return photo_list_cache.get(album);
}

E<IDWithPathRef> ImageSource::albums(const std::string& album)
{
    if(shouldExcludeAlbumFromParent(album))
    {
        return std::unexpected("Not found");
    }

    return album_list_cache.get(album);
}

std::optional<fs::path> ImageSource::image(const std::string& id)
{
    spdlog::debug("Getting image with ID {}...", id);
    auto album_path = fs::path(id).parent_path();
    auto imgs = images(album_path.string());
    if(!imgs.has_value())
    {
        return std::nullopt;
    }
    auto found = imgs->get().find(id);
    if(found == imgs->get().end())
    {
        return std::nullopt;
    }
    return found->second;
}

E<std::filesystem::path> ImageSource::getThumb(const std::string& id)
{
    auto path = image(id);
    if(path.has_value())
    {
        return thumb_manager.get(*path);
    }
    else
    {
        return std::unexpected("Photo not found");
    }
}

E<std::filesystem::path> ImageSource::getPresent(const std::string& id)
{
    auto path = image(id);
    if(path.has_value())
    {
        return present_manager.get(*path);
    }
    else
    {
        return std::unexpected("Photo not found");
    }
}

E<nlohmann::json> ImageSource::getMetadata(const std::string& id)
{
    auto photo_path = image(id);
    if(!photo_path.has_value())
    {
        return std::unexpected("Photo not found");
    }
    auto path = metadata_manager.get(*photo_path);
    if(!path.has_value())
    {
        return std::unexpected(path.error());
    }

    nlohmann::json data = nlohmann::json::parse(std::ifstream(*path), nullptr,
                                                false);
    if(data.is_discarded())
    {
        return std::unexpected("Invalid JSON");
    }
    return data;
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
    if(!imgs.has_value() || imgs->get().empty())
    {
        return std::nullopt;
    }

    return std::min_element(std::begin(imgs->get()), std::end(imgs->get()))
        ->first;
}
