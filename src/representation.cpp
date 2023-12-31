#include <expected>
#include <filesystem>
#include <format>
#include <string>

#include <stdint.h>

#include <Magick++.h>
#include <spdlog/spdlog.h>

#include "representation.hpp"
#include "utils.hpp"

namespace fs = std::filesystem;

E<void> imgResize(const std::string& source, const std::string& result,
                  int quality, uint32_t size)
{
    Magick::Image img;
    try
    {
        img.read(source);
    }
    catch(Magick::WarningCoder&) {}
    catch(Magick::Warning&) {}
    catch(Magick::ErrorFileOpen&)
    {
        return std::unexpected(std::format("Failed to open image {}.", source));
    }
    img.strip();
    img.resize(std::format("{}x{}>", size, size));
    img.quality(quality);
    img.write(result);

    return {};
}

ReprManager::ReprManager(Representation::Type type, const Configuration& conf)
        : repr_type(type), config(conf)
{
}

std::filesystem::path ReprManager::getPath(const fs::path& path)
{
    fs::path base_name = path.stem();
    fs::path data_dir = path.parent_path() / RUNTIME_DATA_DIR;
    std::string_view ext;
    switch(repr_type)
    {
    case Representation::THUMB:
        ext = ImageFormat::toExt(config.thumb_format);
        break;
    case Representation::PRESENT:
        ext = ImageFormat::toExt(config.present_format);
        break;
    }
    return data_dir / std::format(
        "{}-{}.{}", base_name.string(), Representation::str(repr_type),
        ext);
}

bool ReprManager::isFresh(const fs::path& path)
{
    return fs::exists(getPath(path));
}

E<void> ReprManager::refresh(const fs::path& path)
{
    fs::path repr_path = getPath(path);
    fs::path dir = repr_path.parent_path();
    if(!fs::exists(dir))
    {
        fs::create_directory(dir);
    }
    std::string path_str = path.string();
    spdlog::debug("Generating presentation for {}...", path_str);
    int quality = 0;
    uint32_t size = 0;
    switch(repr_type)
    {
    case Representation::THUMB:
        quality = config.thumb_quality;
        size = config.thumb_size;
        break;
    case Representation::PRESENT:
        quality = config.present_quality;
        size = config.present_size;
        break;
    }
    return imgResize(std::move(path_str), repr_path.string(), quality, size);
}
