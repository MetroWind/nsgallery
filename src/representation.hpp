#pragma once

#include <optional>
#include <string_view>
#include <filesystem>

#include "config.hpp"
#include "file_cache.hpp"
#include "representation.hpp"
#include "utils.hpp"

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

    static std::string_view str(Type type)
    {
        switch(type)
        {
        case THUMB:
            return "thumb";
        case PRESENT:
            return "present";
        }
        std::unreachable();
    }
};

class ReprManager: public FileCache
{
public:
    ReprManager(Representation::Type type, const Configuration& conf);
    ~ReprManager() override = default;

protected:
    std::filesystem::path getPath(const std::filesystem::path& path) override;
    bool isFresh(const std::filesystem::path& path) override;
    E<void> refresh(const std::filesystem::path& path) override;

private:
    Representation::Type repr_type;
    const Configuration& config;
};
