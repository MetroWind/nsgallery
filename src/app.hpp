#pragma once

#include <string>
#include <string_view>
#include <format>

#include <httplib.h>
#include <spdlog/spdlog.h>
#include <inja.hpp>

#include "utils.hpp"
#include "config.hpp"
#include "image.hpp"
#include "image_source.hpp"

inline std::string urlForAlbum(const std::string& id,
                               const Configuration& config)
{
    return std::string("/a/") + id;
}

inline std::string urlForPhoto(const std::string& id,
                               const Configuration& config)
{
    return std::string("/p/") + id;
}

inline std::string urlForRepr(const std::string& id,
                              Representation::Type repr,
                              const Configuration& config)
{
    switch(repr)
    {
    case Representation::THUMB:
        return std::format("/repr/{}-thumb.avif", id);
    case Representation::PRESENT:
        return std::format("/repr/{}-present.avif", id);
    default:
        return {};
    }
}

inline std::string urlForStatic(const std::string& path,
                                const Configuration& config)
{
    return std::string("/static/") + path;
}

class App
{
public:
    App() = delete;
    explicit App(const Configuration& conf);

    void handleIndex(httplib::Response& res) const;
    void handleAlbum(const std::string& id, httplib::Response& res);
    void handlePhoto(const std::string& id, httplib::Response& res);
    void handleRepresentation(const std::string& path, httplib::Response& res);
    void start();

private:
    const Configuration config;
    inja::Environment templates;
    ImageSource image_source;
};
