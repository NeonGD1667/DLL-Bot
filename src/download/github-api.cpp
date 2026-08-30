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
        .listen(
            [callback](web::WebResponse response) {
                auto json = response.json();

                if (!json) {
                    callback(Err("Failed to parse GitHub response"));
                    return;
                }

                auto data = json.unwrap();

                auto tagName = data["tag_name"].asString();
                auto name = data["name"].asString();
                auto body = data["body"].asString();
                auto prerelease = data["prerelease"].asBool();
                auto draft = data["draft"].asBool();

                if (!tagName || !name || !body || !prerelease || !draft) {
                    callback(Err("Invalid GitHub release JSON"));
                    return;
                }

                Release release;
                release.tagName = tagName.unwrap();
                release.name = name.unwrap();
                release.body = body.unwrap();
                release.prerelease = prerelease.unwrap();
                release.draft = draft.unwrap();

                callback(Ok(std::move(release)));
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
        .listen(
            [callback](web::WebResponse response) {
                callback(Ok(std::move(response)));
            },
            [callback](std::string const& error) {
                callback(Err(error));
            }
        );
}

} // namespace github
