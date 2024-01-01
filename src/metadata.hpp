#pragma once

#include <filesystem>

#include "file_cache.hpp"
#include "utils.hpp"
#include "config.hpp"

class MetadataManager: public FileCache
{
public:
    MetadataManager() = default;
    explicit MetadataManager(const Configuration& conf);
    ~MetadataManager() override = default;

protected:
    std::filesystem::path getPath(const std::filesystem::path& path) override;
    bool isFresh(const std::filesystem::path& path) override;
    E<void> refresh(const std::filesystem::path& path) override;

private:
    const Configuration& config;
};
