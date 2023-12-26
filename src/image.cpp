#include <expected>
#include <format>
#include <filesystem>

#include <Magick++.h>
#include <spdlog/spdlog.h>

#include "config.hpp"
#include "utils.hpp"
#include "image.hpp"

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

std::filesystem::path ImageFile::getThumb() const
{
    fs::path base_name = fs::path(id).filename();
    fs::path data_dir = base_dir / fs::path(id).parent_path() /
        RUNTIME_DATA_DIR;
    fs::path thumb_file =
        data_dir / std::format("{}-thumb.{}", base_name.string(),
                               ImageFormat::toExt(config.thumb_format));
    if(!fs::exists(thumb_file))
    {
        if(!fs::exists(data_dir))
        {
            fs::create_directory(data_dir);
        }
        spdlog::debug("Generating thumbnail for {}...", id);
        imgResize(path.string(), thumb_file.string(),
                  config.thumb_quality, config.thumb_size);
    }
    return thumb_file;
}

std::filesystem::path ImageFile::getPresent() const
{
    fs::path base_name = fs::path(id).filename();
    fs::path data_dir = base_dir / fs::path(id).parent_path() /
        RUNTIME_DATA_DIR;
    fs::path present_file =
        data_dir / std::format("{}-present.{}", base_name.string(),
                               ImageFormat::toExt(config.present_format));
    if(!fs::exists(present_file))
    {
        if(!fs::exists(data_dir))
        {
            fs::create_directory(data_dir);
        }
        spdlog::debug("Generating presentation for {}...", id);
        imgResize(path.string(), present_file.string(),
                  config.present_quality, config.present_size);
    }
    return present_file;
}
