#pragma once

#include <filesystem>
#include <string_view>

#include "utils.hpp"

class FileCache
{
public:
    virtual ~FileCache() = default;
    E<std::filesystem::path> get(const std::filesystem::path& path)
    {
        if(isFresh(path))
        {
            return getPath(path);
        }
        else
        {
            E<void> status = refresh(path);
            if(status.has_value())
            {
                return getPath(path);
            }
            else
            {
                return std::unexpected(status.error());
            }
        }
    }

protected:
    virtual std::filesystem::path getPath(const std::filesystem::path& path) = 0;
    virtual bool isFresh(const std::filesystem::path& path) = 0;
    virtual E<void> refresh(const std::filesystem::path& path) = 0;
};
