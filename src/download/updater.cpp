#include "updater.hpp"
#include "github-api.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/Dirs.hpp>
#include <Geode/loader/ModMetadata.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

namespace updater {

static constexpr char const* API_URL =
    "https://api.github.com/repos/Homeless-Team/Dll-Bot/releases/latest";

static constexpr char const* MOD_ID =
    "homeless.dll-bot";

static constexpr char const* MOD_FILE =
    "homeless.dll-bot.geode";

static std::string s_latestDownloadURL;

static void showToast(
    std::string const& message,
    NotificationIcon icon,
    float time = 2.f
) {
    Loader::get()->queueInMainThread(
        [message, icon, time] {
            Notification::create(message, icon, time)->show();
        }
    );
}

void download() {
    if (s_latestDownloadURL.empty()) {
        showToast(
            "No update available.",
            NotificationIcon::Error
        );
        return;
    }

    auto loadingToast = Notification::create(
        "Downloading DLL Bot update...",
        NotificationIcon::Loading,
        10.f
    );

    loadingToast->show();

    github::download(
        s_latestDownloadURL,
        [loadingToast](Result<ByteVector> result) {
            Loader::get()->queueInMainThread(
                [loadingToast] {
                    loadingToast->hide();
                }
            );

            if (!result) {
                log::warn(
                    "Failed to download DLL Bot update: {}",
                    result.unwrapErr()
                );

                showToast(
                    "Failed to download DLL Bot update",
                    NotificationIcon::Error
                );

                return;
            }

            auto tmpPath =
                dirs::getTempDir() /
                (std::string(MOD_FILE) + ".tmp");

            auto data = std::move(result).unwrap();

            auto writeTmp =
                utils::file::writeBinary(tmpPath, data);

            if (!writeTmp) {
                log::warn(
                    "Failed to save DLL Bot update: {}",
                    writeTmp.unwrapErr()
                );

                showToast(
                    "Failed to save DLL Bot update",
                    NotificationIcon::Error
                );

                return;
            }

            auto metadata =
                ModMetadata::createFromGeodeFile(tmpPath);

            if (
                metadata.hasErrors() ||
                metadata.getID() != MOD_ID
            ) {
                for (auto const& error : metadata.getErrors()) {
                    log::warn(
                        "Invalid DLL Bot update: {}",
                        error
                    );
                }

                std::error_code ec;
                std::filesystem::remove(tmpPath, ec);

                showToast(
                    "Downloaded DLL Bot update is invalid",
                    NotificationIcon::Error
                );

                return;
            }

            auto targetPath =
                dirs::getModsDir() / MOD_FILE;

            auto installedPath =
                Mod::get()->getPackagePath();

            std::error_code ec;

            if (
                !installedPath.empty() &&
                installedPath != targetPath
            ) {
                std::filesystem::remove(
                    installedPath,
                    ec
                );

                if (
                    ec ||
                    std::filesystem::exists(installedPath, ec)
                ) {
                    log::warn(
                        "Failed to remove old DLL Bot: {}",
                        ec.message()
                    );

                    std::filesystem::remove(tmpPath, ec);

                    showToast(
                        "Failed to replace DLL Bot",
                        NotificationIcon::Error
                    );

                    return;
                }
            }

            ec.clear();

            std::filesystem::remove(
                targetPath,
                ec
            );

            if (
                ec ||
                std::filesystem::exists(targetPath, ec)
            ) {
                log::warn(
                    "Failed to remove DLL Bot target: {}",
                    ec.message()
                );

                std::filesystem::remove(tmpPath, ec);

                showToast(
                    "Failed to replace DLL Bot",
                    NotificationIcon::Error
                );

                return;
            }

            ec.clear();

            std::filesystem::rename(
                tmpPath,
                targetPath,
                ec
            );

            if (ec) {
                log::warn(
                    "Failed to install DLL Bot update: {}",
                    ec.message()
                );

                std::filesystem::remove(tmpPath, ec);

                showToast(
                    "Failed to install DLL Bot update",
                    NotificationIcon::Error
                );

                return;
            }

            Loader::get()->queueInMainThread([] {
                FLAlertLayer::create(
                    "Update Installed",
                    "DLL Bot has been updated.\n"
                    "Please restart Geometry Dash to apply the update.",
                    "OK"
                )->show();
            });
        }
    );
}

void check(bool notifyIfCurrent) {
    github::get(
        API_URL,
        [notifyIfCurrent](Result<github::WebResponse> result) {
            if (!result) {
                if (notifyIfCurrent) {
                    showToast(
                        "Failed to check for DLL Bot updates",
                        NotificationIcon::Error
                    );
                }

                return;
            }

            auto json = result.unwrap().json();

            if (!json || !json.unwrap().isObject()) {
                if (notifyIfCurrent) {
                    showToast(
                        "Invalid GitHub response",
                        NotificationIcon::Error
                    );
                }

                return;
            }

            auto const& release = json.unwrap();

            auto tag = release["tag_name"].asString();
            auto prerelease = release["prerelease"].asBool();
            auto draft = release["draft"].asBool();
            auto assets = release["assets"];

            if (
                !tag ||
                !prerelease ||
                !draft
            ) {
                return;
            }

            if (
                prerelease.unwrap() ||
                draft.unwrap()
            ) {
                return;
            }

            std::string downloadURL;

            if (assets.isArray()) {
                for (auto const& asset : assets) {
                    auto name =
                        asset["name"].asString();

                    auto browserURL =
                        asset["browser_download_url"].asString();

                    if (!name || !browserURL)
                        continue;

                    auto filename = name.unwrap();

                    if (
                        filename.ends_with(".geode") &&
                        filename == MOD_FILE
                    ) {
                        downloadURL =
                            browserURL.unwrap();

                        break;
                    }
                }
            }

            if (downloadURL.empty())
                return;

            std::string latestVersion =
                tag.unwrap();

            std::string localVersion =
                Mod::get()
                    ->getVersion()
                    .toNonVString();

            if (latestVersion == localVersion) {
                if (notifyIfCurrent) {
                    showToast(
                        "DLL Bot is already up to date",
                        NotificationIcon::Success
                    );
                }

                return;
            }

            s_latestDownloadURL =
                std::move(downloadURL);

            Loader::get()->queueInMainThread(
                [localVersion, latestVersion] {
                    createQuickPopup(
                        "Update Available",
                        "A new version of <cy>DLL Bot</c> "
                        "is available!\n\n"
                        "Current: <cr>" +
                        localVersion +
                        "</c>\nLatest: <cg>" +
                        latestVersion +
                        "</c>",
                        "Close",
                        "Update",
                        [](auto, bool btn2) {
                            if (btn2) {
                                updater::download();
                            }
                        }
                    );
                }
            );
        }
    );
}

}