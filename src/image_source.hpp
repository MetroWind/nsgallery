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

class ImageSource
{
public:
    ImageSource() = delete;
    explicit ImageSource(const Configuration& conf)
            : config(conf), dir(conf.photo_root_dir)
    {
    }

    // Return a collection of image IDs.
    E<std::vector<ImageFile>> images(const std::string& album);
    // Return a collection of sub-album IDs.
    E<std::vector<std::string>> albums(std::string_view album) const;

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

    // TODO: check mtime.
    // A map from an album ID to the relative image paths directly in this album.
    std::unordered_map<std::string, std::vector<std::filesystem::path>> image_list_cache;
    std::shared_mutex image_list_lock;
    // A map from an album ID to the relative album paths directly in this album.
    std::unordered_map<std::string, std::vector<std::filesystem::path>> album_list_cache;
    std::shared_mutex album_list_lock;
};
