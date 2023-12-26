#pragma once
#include <string_view>
#include <vector>
#include <unordered_set>
#include <string_view>
#include <optional>
#include <filesystem>
#include <expected>
#include <shared_mutex>

#include "config.hpp"
#include "image.hpp"
#include "utils.hpp"

constexpr std::string_view ALBUM_CONFIG_FILE = ".nsgallery-config.yaml";

class AlbumConfig
{
public:
    enum ItemStatus { SHOW, HIDE, EXCLUDE };

    static AlbumConfig fromYamlOrDefault(const std::filesystem::path& file);
    static std::string statusToStr(ItemStatus);
    ItemStatus getItmeStatus(const std::string& file_base_name);
    std::optional<std::string> getCover() const { return cover; }

private:
    AlbumConfig() = default;

    std::optional<std::string> cover;
    std::unordered_set<std::string> hides;
    std::unordered_set<std::string> excludes;
    std::unordered_set<std::string> includes;
};

struct IDWithName
{
    std::string id;
    std::string name;
};

enum class CacheStatus { STALE, FRESH };

class ItemListCache
{
public:
    ItemListCache() = default;
    ItemListCache(const ItemListCache&) = delete;
    ItemListCache(ItemListCache&&) = default;
    ItemListCache& operator=(const ItemListCache&) = delete;
    ItemListCache& operator=(ItemListCache&&) = default;

    const std::vector<std::filesystem::path>* get(const std::string& key);
    void setGetFresh(std::function<std::vector<std::filesystem::path>(const std::string&)> func)
    {
        refresh = func;
    }
    void setDetectStale(std::function<CacheStatus(const std::string&)> func)
    {
        detectStale = func;
    }

private:
    std::unordered_map<std::string, std::vector<std::filesystem::path>> cache;
    std::shared_mutex lock;
    std::function<std::vector<std::filesystem::path>(const std::string&)> refresh;
    std::function<CacheStatus(const std::string&)> detectStale;
};

class ImageSource
{
public:
    ImageSource() = delete;
    explicit ImageSource(const Configuration& conf);

    // Return a collection of image IDs.
    E<std::vector<ImageFile>> images(const std::string& album);
    // Return a collection of sub-album IDs.
    E<std::vector<std::string>> albums(const std::string& album);

    // Find an image by ID.
    std::optional<ImageFile> image(std::string_view id);

    AlbumConfig::ItemStatus imageStatus(std::string_view id) const;
    AlbumConfig::ItemStatus albumStatus(std::string_view id) const;

    // Return the ID of the image that is used as album cover. It is
    // possible to not find a suitable cover (e.g. empty album). In
    // that case returns nullopt.
    std::optional<std::string> albumCover(const std::string& album_id);

    // Return a link of ancesters of the item with “id”, from the
    // album directly under root to the directly containing album.
    std::vector<IDWithName> navChain(std::string_view id) const;

private:
    bool shouldExcludeImageFromParent(std::string_view id) const;
    bool shouldExcludeAlbumFromParent(std::string_view id) const;

    const Configuration& config;
    const std::filesystem::path dir;

    ItemListCache photo_list_cache;
    ItemListCache album_list_cache;
};
