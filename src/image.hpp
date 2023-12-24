#pragma once

#include <string_view>
#include <string>

#include <stdint.h>

#include <nlohmann/json.hpp>

#include "utils.hpp"
#include "config.hpp"

constexpr std::string_view RUNTIME_DATA_DIR = ".nsgallery-data";

E<void> imgResize(const std::string& source, const std::string& result,
                  int quality, uint32_t size);

class Representation
{
public:
    enum Type { THUMB, PRESENT };
    static std::optional<Type> fromStr(std::string_view s)
    {
        if(s == "thumb")
        {
            return THUMB;
        }
        else if(s == "present")
        {
            return PRESENT;
        }
        else
        {
            return std::nullopt;
        }
    }
};

class ImageFile
{
public:
    const std::filesystem::path path;
    const std::string id;

    ImageFile() = delete;
    ImageFile(const std::filesystem::path& base_dir_, std::string id_,
              const std::filesystem::path& path_, const Configuration& conf)
            : path(path_), id(id_), base_dir(base_dir_), config(conf) {}

    std::string album() const
    {
        return std::filesystem::path(id).parent_path().string();
    }

    std::filesystem::path getThumb() const;
    std::filesystem::path getPresent() const;

    nlohmann::json json() const
    {
        return {{"id", id}};
    }

private:
    const std::filesystem::path& base_dir;
    const Configuration& config;
};
