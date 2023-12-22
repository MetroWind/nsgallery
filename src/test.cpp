#include <filesystem>
#include <fstream>
#include <limits>
#include <expected>
#include <utility>

#include <ryml.hpp>
#include <ryml_std.hpp>

#include <inja.hpp>
#include <nlohmann/json.hpp>

#include <oatpp/web/server/HttpConnectionHandler.hpp>

#include <oatpp/network/Server.hpp>
#include <oatpp/network/tcp/server/ConnectionProvider.hpp>

class Configuration
{
public:
    std::string template_dir = "./templates";

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
        if(tree["template-dir"].has_key())
        {
            auto value = tree["template-dir"].val();
            config.template_dir = std::string(value.begin(), value.end());
        }

        return std::expected<Configuration, std::string>
            {std::in_place, std::move(config)};
    }
};

class ImageFile
{
public:
    ImageFile() = delete;
    ImageFile(const std::filesystem::path& dir_,
              std::string&& file_)
        : dir(dir_), file(std::move(file_))
    {
    }

    std::filesystem::path path() const
    {
        std::filesystem::path p = dir;
        p.append(file);
        return p;
    }

private:
    const std::filesystem::path& dir;
    const std::string file;
};

class ImageAlbum
{
public:
    ImageAlbum() = delete;
    explicit ImageAlbum(const std::filesystem::path& dir_)
        : dir(dir_)
    {
    }

    std::vector<ImageFile> images() const
    {
        std::vector<ImageFile> result;
        for(const std::filesystem::directory_entry& entry:
                std::filesystem::directory_iterator(dir))
        {
            if(!(entry.is_regular_file() || entry.is_symlink()))
            {
                continue;
            }
            const auto& path = entry.path();
            std::string ext = path.extension().string();
            // Convert file extension to lower case. WARNING: This
            // assumes ASCII extension names!
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if(ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".tif"
               || ext == ".tiff" || ext == ".webp" || ext == ".avif")
            {
                result.emplace_back(dir, path.filename().string());
            }
        }
        return result;
    }

private:
    const std::filesystem::path dir;
};

class StaticFilesManager
{
public:
    oatpp::String getFile(const oatpp::String& path)
    {
        if (!path)
        {
            return nullptr;
        }
        std::lock_guard<oatpp::concurrency::SpinLock> lock(m_lock);
        auto& file = m_cache[path];
        if (file)
        {
            return file;
        }
        oatpp::String filename = path;
        file = oatpp::String::loadFromFile(filename->c_str());
        return file;
    }

private:
    oatpp::concurrency::SpinLock m_lock;
    std::unordered_map<oatpp::String, oatpp::String> m_cache;
};

class IndexHandler : public oatpp::web::server::HttpRequestHandler
{
public:
    explicit IndexHandler(inja::Environment& templates_)
        : templates(templates_)
    {
    }

    std::shared_ptr<OutgoingResponse>
    handle(const std::shared_ptr<IncomingRequest>& request) override
    {
        ImageAlbum album("/Volumes/stuff/Pictures/fuli/ai");
        nlohmann::json fe_data;
        fe_data["images"] = nlohmann::json::value_t::array;
        for(const ImageFile& img: album.images())
        {
            fe_data["images"].push_back(img.path().string());
        }
        std::string result = templates.render_file(
            "index.html", std::move(fe_data));
        return ResponseFactory::createResponse(Status::CODE_200, result);
    }
private:
    inja::Environment& templates;
};

void run()
{
    Configuration config;
    config.template_dir = "../templates/";
    inja::Environment templates(config.template_dir);

    auto router = oatpp::web::server::HttpRouter::createShared();
    router->route("GET", "/hello", std::make_shared<IndexHandler>(templates));
    auto handler =
        oatpp::web::server::HttpConnectionHandler::createShared(router);
    auto connection =
        oatpp::network::tcp::server::ConnectionProvider::createShared(
            {"localhost", 8000, oatpp::network::Address::IP_4});

    oatpp::network::Server server(connection, handler);
    OATPP_LOGI("MyApp", "Server running on port %s",
               reinterpret_cast<const char*>(
                   connection->getProperty("port").getData()));
    server.run();
}

int main()
{
    oatpp::base::Environment::init();
    run();
    oatpp::base::Environment::destroy();

    return 0;

}
