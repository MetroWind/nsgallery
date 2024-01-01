#include <string>
#include <regex>

#include <inja.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "app.hpp"
#include "config.hpp"
#include "image_source.hpp"

nlohmann::json navChainToJson(std::vector<IDWithName>&& chain)
{
    nlohmann::json result = nlohmann::json::value_t::array;
    for(IDWithName& seg: chain)
    {
        result.push_back(
            {{ "id", std::move(seg.id)}, {"name", std::move(seg.name)}});
    }
    return result;
}

App::App(const Configuration& conf)
        : config(conf), templates(conf.template_dir), image_source(conf)
{
    templates.add_callback("url_for_album", 1, [&](const inja::Arguments& args)
    {
        return urlForAlbum(args.at(0)->get_ref<const std::string&>(), config);
    });

    templates.add_callback("url_for_photo", 1, [&](const inja::Arguments& args)
    {
        return urlForPhoto(args.at(0)->get_ref<const std::string&>(), config);
    });

    templates.add_callback("url_for_repr", 2, [&](const inja::Arguments& args)
    {
        auto repr = Representation::fromStr(
            args.at(1)->get_ref<const std::string&>());
        if(!repr.has_value())
        {
            return std::string();
        }
        return urlForRepr(args.at(0)->get_ref<const std::string&>(), *repr,
                          config);
    });

    templates.add_callback("url_for_static", 1, [&](const inja::Arguments& args)
    {
        return urlForStatic(args.at(0)->get_ref<const std::string&>(), config);
    });
}

void App::handleIndex(httplib::Response& res) const
{
    res.set_redirect(urlForAlbum("", config));
}

void App::handleAlbum(const std::string& id, httplib::Response& res)
{
    nlohmann::json fe_data;
    fe_data["images"] = nlohmann::json::value_t::array;
    fe_data["albums"] = nlohmann::json::value_t::array;
    fe_data["id"] = id;
    fe_data["name"] = fs::path(id).filename().string();
    fe_data["thumb_size"] = config.thumb_size;
    const auto albums = image_source.albums(id);
    if(!albums.has_value())
    {
        res.status = httplib::StatusCode::NotFound_404;
        res.set_content("Not found.", "text/plain");
        return;
    }

    for(const std::string& album: orderedIDsFromIDWithPath(*albums))
    {
        auto cover = image_source.albumCover(album);
        if(cover.has_value())
        {
            fe_data["albums"].push_back(
                {{"id", album},
                 {"name", std::filesystem::path(album).filename().string()},
                 {"cover", *cover},
                 {"cover_type", "image"}});
        }
        else
        {
            fe_data["albums"].push_back(
                {{"id", album},
                 {"name", std::filesystem::path(album).filename().string()},
                 {"cover", "default-cover.svg"},
                 {"cover_type", "static"}});
        }
    }
    auto images = image_source.images(id);
    if(!images.has_value())
    {
        res.status = httplib::StatusCode::NotFound_404;
        res.set_content("Not found.", "text/plain");
        return;
    }
    for(const std::string& img: orderedIDsFromIDWithPath(*images))
    {
        fe_data["images"].push_back({{ "id", img }});
    }
    fe_data["navigation"] = navChainToJson(image_source.navChain(id));
    std::string result = templates.render_file(
        "index.html", std::move(fe_data));
    res.set_content(result, "text/html");
}

void App::handlePhoto(const std::string& id, httplib::Response& res)
{
    if(!image_source.image(id).has_value())
    {
        res.status = httplib::StatusCode::BadRequest_400;
        res.set_content("Image not found.", "text/plain");
        return;
    }
    nlohmann::json fe_data;
    fe_data["id"] = id;
    fe_data["name"] = fs::path(id).filename().string();
    auto metadata = image_source.getMetadata(id);
    if(metadata.has_value())
    {
        fe_data["metadata"] = *std::move(metadata);
    }
    fe_data["navigation"] = navChainToJson(image_source.navChain(id));
    std::string result = templates.render_file(
        "photo.html", std::move(fe_data));
    res.set_content(result, "text/html");
}

void App::handleRepresentation(const std::string& path, httplib::Response& res)
{
    std::regex p(std::format(R"((.*)-(thumb\.{}|present\.{}))",
                             ImageFormat::toExt(config.thumb_format),
                             ImageFormat::toExt(config.present_format)));
    std::smatch match;
    if(!std::regex_match(path, match, p))
    {
        res.status = httplib::StatusCode::BadRequest_400;
        res.set_content("Invalid path of representation.", "text/plain");
        return;
    }

    const std::string& id = match[1];
    const std::string& repr_str = match[2];
    Representation::Type repr;
    if(repr_str.starts_with("thumb."))
    {
        repr = Representation::THUMB;
    }
    else if(repr_str.starts_with("present."))
    {
        repr = Representation::PRESENT;
    }
    else
    {
        res.status = httplib::StatusCode::BadRequest_400;
        res.set_content("Unknown representation.", "text/plain");
        return;
    }

    E<std::vector<char>> content;
    std::string content_type;
    switch(repr)
    {
    case Representation::THUMB:
    {
        auto thumb = image_source.getThumb(id);
        if(!thumb.has_value())
        {
            res.status = httplib::StatusCode::InternalServerError_500;
            res.set_content("Failed to get thumbnail.", "text/plain");
            return;
        }
        content = readFile(*thumb);
        content_type = ImageFormat::contentType(config.thumb_format);
        break;
    }
    case Representation::PRESENT:
    {
        auto present = image_source.getPresent(id);
        if(!present.has_value())
        {
            res.status = httplib::StatusCode::InternalServerError_500;
            res.set_content("Failed to get presentation.", "text/plain");
            return;
        }
        content = readFile(*present);
        content_type = ImageFormat::contentType(config.present_format);
        break;
    }
    }

    if(content.has_value())
    {
        res.set_content(content->data(), content->size(),
                        std::move(content_type));
    }
    else
    {
        spdlog::error(content.error());
        res.set_content("Internal error", "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
}

void App::start()
{
    httplib::Server server;
    spdlog::info("Mounting static dir at {}...", config.static_dir);
    auto ret = server.set_mount_point("/static", config.static_dir);
    if (!ret)
    {
        spdlog::error("Failed to mount static");
    }

    server.Get("/", [&]([[maybe_unused]] const httplib::Request& req,
                        httplib::Response& res)
    {
        handleIndex(res);
    });
    server.Get("/repr/(.+)",
               [&](const httplib::Request& req, httplib:: Response& res)
               {
                   handleRepresentation(req.matches[1], res);
               });
    server.Get("/a(/.*)?", [&](const httplib::Request& req,
                               httplib::Response& res)
    {
        const std::string& match = req.matches[1];
        if(match.starts_with("/"))
        {
            handleAlbum(match.substr(1), res);
        }
        else
        {
            handleAlbum(match, res);
        }
    });

    server.Get("/p/(.+)", [&](const httplib::Request& req,
                              httplib::Response& res)
    {
        handlePhoto(req.matches[1], res);
    });

    spdlog::info("Listening at http://{}:{}/...", config.listen_address,
                 config.listen_port);
    server.listen(config.listen_address, config.listen_port);
}
