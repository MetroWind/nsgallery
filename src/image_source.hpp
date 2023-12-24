#pragma once
#include <string_view>
#include <vector>
#include <unordered_set>
#include <string_view>
#include <optional>
#include <filesystem>
#include <expected>

#include "config.hpp"
#include "image.hpp"
#include "utils.hpp"

constexpr std::string_view ALBUM_CONFIG_FILE = ".nsgallery-config.yaml";

class AlbumConfig
{
public:
    enum ItemStatus { SHOW, HIDE, EXCLUDE };

    static AlbumConfig fromYamlOrDefault(const std::filesystem::path& file);
    ItemStatus getItmeStatus(const std::string& file_base_name);
private:
    AlbumConfig() = default;

    std::unordered_set<std::string> hides;
    std::unordered_set<std::string> excludes;
    std::unordered_set<std::string> includes;
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
    E<std::vector<ImageFile>> images(std::string_view album) const;
    // Return a collection of sub-album IDs.
    E<std::vector<std::string>> albums(std::string_view album) const;

    // Find an image by ID.
    std::optional<ImageFile> image(std::string_view id) const;

private:
    const Configuration& config;
    const std::filesystem::path dir;
};
