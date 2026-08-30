#include "github-api.hpp"

using namespace geode::prelude;

namespace github {

void API::getLatestRelease(
    std::string const& owner,
    std::string const& repo,
    std::function<void(geode::Result<Release>)> callback
) {
    auto url = fmt::format(
        "https://api.github.com/repos/{}/{}/releases/latest",
        owner,
        repo
    );

    web::WebRequest()
        .userAgent("DLL-Bot")
        .get(url)
        .map([](web::WebResponse const& response) -> Result<Release> {
            auto json = response.json();

            if (!json) {
                return Err("Failed to parse GitHub response");
            }

            auto const& data = json.unwrap();

            Release release;
            release.tagName = data["tag_name"].asString();
            release.name = data["name"].asString();
            release.body = data["body"].asString();
            release.prerelease = data["prerelease"].asBool();
            release.draft = data["draft"].asBool();

            return Ok(release);
        })
        .expect(
            [callback](Release release) {
                callback(Ok(release));
            },
            [callback](std::string const& error) {
                callback(Err(error));
            }
        );
}

void API::download(
    std::string const& url,
    std::function<void(geode::Result<geode::utils::web::WebResponse>)> callback
) {
    web::WebRequest()
        .userAgent("DLL-Bot")
        .get(url)
        .expect(
            [callback](web::WebResponse response) {
                callback(Ok(std::move(response)));
            },
            [callback](std::string const& error) {
                callback(Err(error));
            }
        );
}

} // namespace github
