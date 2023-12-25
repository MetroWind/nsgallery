#pragma once

#include <charconv>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <ryml.hpp>
#include <ryml_std.hpp>

template<class T>
using E = std::expected<T, std::string>;

template<class T>
inline bool getYamlValue(ryml::ConstNodeRef node, T& result)
{
    auto value = node.val();
    auto status = std::from_chars(value.begin(), value.end(), result);
    return status.ec == std::errc();
}

inline E<std::vector<char>> readFile(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    std::vector<char> content;
    content.reserve(102400);
    content.assign(std::istreambuf_iterator<char>(f),
                   std::istreambuf_iterator<char>());
    if(f.bad() || f.fail())
    {
        return std::unexpected(std::format("Failed to read file {}", path.string()));
    }

    return content;
}

// Convert a string to lower case, assuming ASCII.
inline std::string asciiLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}
