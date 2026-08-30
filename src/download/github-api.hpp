#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

namespace github {

struct Release {
    std::string tagName;
    std::string name;
    std::string body;
    bool prerelease = false;
    bool draft = false;
};

class API {
public:
    static void getLatestRelease(
        std::string const& owner,
        std::string const& repo,
        std::function<void(geode::Result<Release>)> callback
    );

    static void download(
        std::string const& url,
        std::function<void(geode::Result<geode::utils::web::WebResponse>)> callback
    );
};

} // namespace github
