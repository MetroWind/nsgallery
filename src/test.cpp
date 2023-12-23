#include <charconv>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <fstream>
#include <limits>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <optional>

#include <stdint.h>

#include <ryml.hpp>
#include <ryml_std.hpp>
#include <inja.hpp>
#include <nlohmann/json.hpp>
#include <Magick++.h>
#include <httplib.h>
// #include <oatpp/web/server/HttpConnectionHandler.hpp>
// #include <oatpp/network/Server.hpp>
// #include <oatpp/network/tcp/server/ConnectionProvider.hpp>
#include <spdlog/spdlog.h>

constexpr std::string_view RUNTIME_DATA_DIR = ".nsgallery-data";

template<class T>
void getYamlValue(ryml::ConstNodeRef node, T& result)
{
    auto value = node.val();
    std::from_chars(value.begin(), value.end(), result);
}

std::vector<char> readFile(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    std::vector<char> content;
    content.reserve(102400);
    content.assign(std::istreambuf_iterator<char>(f),
                   std::istreambuf_iterator<char>());
    return content;
}

class Configuration
{
public:
    std::string photo_root_dir = ".";
    std::string template_dir = "./templates";
    uint32_t thumb_size = 128;
    int thumb_quality = 80;
    uint32_t present_size = 1280;
    int present_quality = 85;

    static std::expected<Configuration, std::string>
    fromYaml(const std::filesystem::path& path)
    {

        std::ifstream file(path);
        file.ignore( std::numeric_limits<std::streamsize>::max() );
        std::streamsize length = file.gcount();
        file.clear();   //  Since ignore will have set eof.
        file.seekg(0, std::ios_base::beg);

        std::vector<char> buffer(length + 1, 0);
        file.read(buffer.data(), length);
        file.close();

        ryml::Tree tree = ryml::parse_in_place(buffer.data());
        Configuration config;
        if(tree["photo-root-dir"].has_key())
        {
            auto value = tree["photo-root-dir"].val();
            config.photo_root_dir = std::string(value.begin(), value.end());
        }
        if(tree["template-dir"].has_key())
        {
            auto value = tree["template-dir"].val();
            config.template_dir = std::string(value.begin(), value.end());
        }
        if(tree["thumb-size"].has_key())
        {
            getYamlValue(tree["thumb-size"], config.thumb_size);
        }
        if(tree["thumb-quality"].has_key())
        {
            getYamlValue(tree["thumb-quality"], config.thumb_quality);
        }
        if(tree["present-size"].has_key())
        {
            getYamlValue(tree["present-size"], config.present_size);
        }
        if(tree["present-quality"].has_key())
        {
            getYamlValue(tree["present-quality"], config.present_quality);
        }

        return std::expected<Configuration, std::string>
            {std::in_place, std::move(config)};
    }
};

void imgResize(const std::string& source, const std::string& result,
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
        return;
    }
    img.strip();
    img.resize(std::format("{}x{}>", size, size));
    img.quality(quality);
    img.write(result);
}

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

    std::filesystem::path getThumb() const
    {
        namespace fs = std::filesystem;
        fs::path base_name = fs::path(id).filename();
        fs::path data_dir = base_dir / fs::path(id).parent_path() /
            RUNTIME_DATA_DIR;
        fs::path thumb_file =
            data_dir / std::format("{}-thumb.avif", base_name.string());
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

    nlohmann::json json() const
    {
        return {{"id", id}};
    }

private:
    const std::filesystem::path& base_dir;
    const Configuration& config;
};

class ImageSource
{
public:
    ImageSource() = delete;
    explicit ImageSource(const Configuration& conf)
            : config(conf), dir(conf.photo_root_dir)
    {
    }

    // Return a collection of image IDs.
    std::vector<ImageFile> images(std::string_view album) const
    {
        std::vector<ImageFile> result;
        spdlog::debug("Listing {}...", (dir / album).string());
        for(const std::filesystem::directory_entry& entry:
                std::filesystem::directory_iterator(dir / album))
        {
            if(!(entry.is_regular_file() || entry.is_symlink()))
            {
                continue;
            }
            const auto& path = entry.path();
            if(path.filename().string().starts_with("."))
            {
                continue;
            }

            std::string ext = path.extension().string();
            // Convert file extension to lower case. WARNING: This
            // assumes ASCII extension names!
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if(ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".tif"
               || ext == ".tiff" || ext == ".webp" || ext == ".avif")
            {
                auto base = path.filename().stem();
                auto id = (std::filesystem::path(album) / base).string();
                result.emplace_back(dir, id, entry.path(), config);
            }
        }
        return result;
    }

    std::optional<ImageFile> image(std::string_view id) const
    {
        auto album_path = std::filesystem::path(id).parent_path();
        auto imgs = images(album_path.string());
        auto found = std::find_if(std::begin(imgs), std::end(imgs),
                     [id](const auto& img) { return img.id == id; });
        if(found == std::end(imgs))
        {
            return std::nullopt;
        }
        return *found;
    }

private:
    const Configuration& config;
    const std::filesystem::path dir;
};

// class StaticFilesManager
// {
// public:
//     oatpp::String getFile(const oatpp::String& path)
//     {
//         if (!path)
//         {
//             return nullptr;
//         }
//         std::lock_guard<oatpp::concurrency::SpinLock> lock(m_lock);
//         auto& file = m_cache[path];
//         if (file)
//         {
//             return file;
//         }
//         oatpp::String filename = path;
//         file = oatpp::String::loadFromFile(filename->c_str());
//         return file;
//     }

// private:
//     oatpp::concurrency::SpinLock m_lock;
//     std::unordered_map<oatpp::String, oatpp::String> m_cache;
// };

// class IndexHandler : public oatpp::web::server::HttpRequestHandler
// {
// public:
//     IndexHandler(inja::Environment& templates_, const Configuration& conf)
//             : templates(templates_), config(conf)
//     {
//     }

//     std::shared_ptr<OutgoingResponse>
//     handle(const std::shared_ptr<IncomingRequest>& request) override
//     {
//         ImageSource album(config);
//         nlohmann::json fe_data;
//         fe_data["images"] = nlohmann::json::value_t::array;
//         for(const ImageFile& img: album.images("."))
//         {
//             fe_data["images"].push_back(img.path.string());
//         }
//         std::string result = templates.render_file(
//             "index.html", std::move(fe_data));
//         return ResponseFactory::createResponse(Status::CODE_200, result);
//     }
// private:
//     inja::Environment& templates;
//     const Configuration& config;
// };

class App
{
public:
    App() = delete;
    explicit App(const Configuration& conf)
            : config(conf), templates(conf.template_dir), image_source(conf) {}

    void handleIndex(httplib::Response& res)
    {
        nlohmann::json fe_data;
        fe_data["images"] = nlohmann::json::value_t::array;
        for(const ImageFile& img: image_source.images(""))
        {
            fe_data["images"].push_back(img.json());
        }
        std::string result = templates.render_file(
            "index.html", std::move(fe_data));
        res.set_content(result, "text/html");
    }

    void handleRepresentation(const std::string& path, httplib::Response& res) const
    {
        std::regex p("(.*)-(thumb|present).avif");
        std::smatch match;
        if(!std::regex_match(path, match, p))
        {
            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content("Invalid path of representation.", "text/plain");
            return;
        }

        std::string id = match[1];
        const std::optional<ImageFile> img = image_source.image(id);
        if(!img.has_value())
        {
            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content("Image not found.", "text/plain");
            return;
        }
        std::vector<char> content = readFile(img->getThumb());
        res.set_content(content.data(), content.size(), "image/avif");
        return;
    }

    void start()
    {
        httplib::Server server;
        server.Get("/", [&](const httplib::Request& _, httplib::Response& res)
        {
            handleIndex(res);
        });
        server.Get(
            "/repr/(.+)",
            [&](const httplib::Request& req, httplib:: Response& res)
            {
                handleRepresentation(req.matches[1], res);
            });

        spdlog::info("Listening at http://localhost:8000/...");
        server.listen("localhost", 8000);
    }

private:
    const Configuration config;
    inja::Environment templates;
    ImageSource image_source;
};

int main(int argc, char** argv)
{
    Magick::InitializeMagick(*argv);
    spdlog::set_level(spdlog::level::debug);

    const Configuration config {
        .photo_root_dir = "/mnt/stuff/Pictures",
        .template_dir = "../templates/",
    };
    App app(config);
    app.start();

    return 0;

}
