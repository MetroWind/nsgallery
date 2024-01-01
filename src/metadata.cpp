#include <string>
#include <expected>
#include <fstream>

#include <stdio.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "metadata.hpp"
#include "utils.hpp"

namespace fs = std::filesystem;

E<std::vector<char>> runOutput(const std::string& command)
{
    // 64KiB block.
    size_t block_size = 65536;
    std::vector<char> output(block_size, 0);
    FILE* proc = popen(command.c_str(), "r");
    if(proc == nullptr)
    {
        return std::unexpected("Failed to run command");
    }
    size_t cursor = 0;
    while(!feof(proc))
    {
        if(output.size() - cursor < block_size)
        {
            output.resize(cursor + block_size);
        }
        cursor += fread(&output[cursor], 1, block_size, proc);
        if(ferror(proc))
        {
            return std::unexpected("Failed to read command output");
        }
    }

    output.resize(cursor);
    if(pclose(proc) != 0)
    {
        return std::unexpected("Command failed");
    }

    return {output};
}

void transferValue(nlohmann::json& src, nlohmann::json& dest, const char* field,
                   const nlohmann::json& false_value)
{
    nlohmann::json& value = src[field];
    if(value != nlohmann::json() && value != false_value)
    {
        spdlog::debug("Moving field {}...", field);
        dest[field] = std::move(value);
    }
    else
    {
        spdlog::debug("Field {} is no good.", field);
    }
}

MetadataManager::MetadataManager(const Configuration& conf)
        : config(conf)
{
}

fs::path MetadataManager::getPath(const fs::path& path)
{
    std::string basename = path.stem().string();
    return path.parent_path() / RUNTIME_DATA_DIR / (basename + "-metadata.json");
}

bool MetadataManager::isFresh(const fs::path& path)
{
    return fs::exists(getPath(path));
}

E<void> MetadataManager::refresh(const fs::path& path)
{
    std::string cmd = std::format(
        "\"{}\" -json -Make -Model -FNumber -ExposureTime -ISO -LensID "
        "-FocalLength -Headline -Title -Caption-Abstract \"{}\"",
        config.exiftool_path, path.string());
    auto output = runOutput(cmd.c_str());
    if(!output.has_value())
    {
        return std::unexpected(output.error());
    }
    nlohmann::json data = nlohmann::json::parse(*output, nullptr, false);
    if(data.is_discarded())
    {
        return std::unexpected("Invalid metadata JSON");
    }

    spdlog::debug("Raw metadata is {}", data.dump());
    nlohmann::json normalized;
    transferValue(data[0], normalized, "Make", nlohmann::json::value_t::string);
    transferValue(data[0], normalized, "Model",
                  nlohmann::json::value_t::string);
    transferValue(data[0], normalized, "FNumber", 0);
    transferValue(data[0], normalized, "ExposureTime",
                  nlohmann::json::value_t::string);
    transferValue(data[0], normalized, "ISO", 0);
    transferValue(data[0], normalized, "FocalLength", "0.0 mm");
    transferValue(data[0], normalized, "LensID",
                  nlohmann::json::value_t::string);
    transferValue(data[0], normalized, "Headline",
                  nlohmann::json::value_t::string);
    transferValue(data[0], normalized, "Title", nlohmann::json::value_t::string);
    transferValue(data[0], normalized, "Caption-Abstract",
                  nlohmann::json::value_t::string);
    spdlog::debug("Normalized metadata is {}", normalized.dump());
    std::ofstream file(getPath(path));
    if(!file)
    {
        return std::unexpected("Failed to open metadata JSON file");
    }

    file << normalized;
    if(!file)
    {
        return std::unexpected("Failed to write metadata JSON file");
    }
    file.close();
    return {};
}
