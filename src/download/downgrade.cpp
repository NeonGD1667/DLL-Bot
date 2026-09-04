#include "downgrade.hpp"
#include "github-api.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>

#include <array>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace downgrade {

static constexpr char const* RELEASES_URL =
    "https://api.github.com/repos/Homeless-Team/Dll-Bot/releases";

static constexpr char const* MOD_FILE =
    "homeless.dll-bot.geode";

// Global lock for the whole downgrade operation.
// This prevents multiple .tmp downloads from being created.
static bool s_downloading = false;

struct ReleaseInfo {
    std::string tag;
    std::string name;
    std::string downloadURL;
    bool prerelease = false;
};

static std::array<int, 3> parseVersion(std::string version) {
    if (!version.empty() &&
        (version[0] == 'v' || version[0] == 'V')) {
        version.erase(0, 1);
    }

    // 3.0.0-beta -> 3.0.0
    // 2.7.0-rc1 -> 2.7.0
    auto dash = version.find('-');
    if (dash != std::string::npos) {
        version.erase(dash);
    }

    std::array<int, 3> result{0, 0, 0};

    std::stringstream stream(version);
    std::string part;

    for (int i = 0; i < 3; ++i) {
        if (!std::getline(stream, part, '.'))
            break;

        try {
            result[i] = std::stoi(part);
        }
        catch (...) {
            result[i] = 0;
        }
    }

    return result;
}

static bool isOlderVersion(
    std::string const& version,
    std::string const& currentVersion
) {
    return parseVersion(version) < parseVersion(currentVersion);
}

static void showError(std::string const& message) {
    FLAlertLayer::create(
        "DLL Bot",
        message.c_str(),
        "OK"
    )->show();
}

static void resetDownloadLock() {
    s_downloading = false;
}

static void installRelease(ReleaseInfo const& release) {
    // Extra protection in case this function is called directly.
    if (s_downloading)
        return;

    // Lock BEFORE starting the HTTP request.
    s_downloading = true;

    github::download(
        release.downloadURL,
        [release](geode::Result<geode::ByteVector> result) {
            auto tempPath =
                Mod::get()->getSaveDir() /
                "DLL-Bot-downgrade.tmp.geode";

            if (!result) {
                if (ghc::filesystem::exists(tempPath)) {
                    std::error_code ec;
                    ghc::filesystem::remove(tempPath, ec);
                }

                resetDownloadLock();

                showError(
                    fmt::format(
                        "Failed to download {}.\n\n{}",
                        release.tag,
                        result.unwrapErr()
                    )
                );
                return;
            }

            auto const& data = result.unwrap();

            if (data.empty()) {
                if (ghc::filesystem::exists(tempPath)) {
                    std::error_code ec;
                    ghc::filesystem::remove(tempPath, ec);
                }

                resetDownloadLock();

                showError(
                    "Downloaded package is empty."
                );
                return;
            }

            // Write to a temporary file first.
            if (!file::writeBinarySafe(tempPath, data)) {
                resetDownloadLock();

                showError(
                    "Failed to write the downloaded package."
                );
                return;
            }

            if (!ghc::filesystem::exists(tempPath)) {
                resetDownloadLock();

                showError(
                    "Downloaded package could not be created."
                );
                return;
            }

            auto targetPath =
                dirs::getModsDir() /
                MOD_FILE;

            auto installedPath =
                Mod::get()->getPackagePath();

            // Remove the currently installed package.
            if (ghc::filesystem::exists(installedPath)) {
                std::error_code ec;

                ghc::filesystem::remove(
                    installedPath,
                    ec
                );

                if (ec) {
                    ghc::filesystem::remove(
                        tempPath,
                        ec
                    );

                    resetDownloadLock();

                    showError(
                        fmt::format(
                            "Failed to remove the current DLL Bot.\n\n{}",
                            ec.message()
                        )
                    );
                    return;
                }
            }

            // Remove an existing target package.
            if (ghc::filesystem::exists(targetPath)) {
                std::error_code ec;

                ghc::filesystem::remove(
                    targetPath,
                    ec
                );

                if (ec) {
                    ghc::filesystem::remove(
                        tempPath,
                        ec
                    );

                    resetDownloadLock();

                    showError(
                        fmt::format(
                            "Failed to replace the DLL Bot package.\n\n{}",
                            ec.message()
                        )
                    );
                    return;
                }
            }

            // Move the downloaded package into the mods directory.
            std::error_code renameError;

            ghc::filesystem::rename(
                tempPath,
                targetPath,
                renameError
            );

            if (renameError) {
                // Fallback for filesystems where rename() fails.
                std::error_code copyError;

                ghc::filesystem::copy_file(
                    tempPath,
                    targetPath,
                    ghc::filesystem::copy_options::overwrite_existing,
                    copyError
                );

                if (copyError) {
                    std::error_code removeError;

                    ghc::filesystem::remove(
                        tempPath,
                        removeError
                    );

                    resetDownloadLock();

                    showError(
                        fmt::format(
                            "Failed to install DLL Bot {}.\n\n{}",
                            release.tag,
                            copyError.message()
                        )
                    );
                    return;
                }

                std::error_code removeError;

                ghc::filesystem::remove(
                    tempPath,
                    removeError
                );
            }

            // Do NOT unlock here.
            // The installation succeeded, so prevent another
            // downgrade operation until Geometry Dash restarts.
            FLAlertLayer::create(
                "DLL Bot",
                fmt::format(
                    "Successfully downgraded to {}.\n\n"
                    "Please restart Geometry Dash to load the new version.",
                    release.tag
                ).c_str(),
                "OK"
            )->show();
        }
    );
}

static void confirmInstall(
    ReleaseInfo const& release,
    CCMenuItemSpriteExtra* button
) {
    if (s_downloading)
        return;

    auto message = fmt::format(
        "Are you sure you want to downgrade DLL Bot to {}?\n\n"
        "Geometry Dash will need to be restarted after installation.",
        release.tag
    );

    auto alert = FLAlertLayer::create(
        "Confirm Downgrade",
        message.c_str(),
        "Cancel",
        "Accept"
    );

    // Accept callback.
    //
    // The button is disabled BEFORE starting the download.
    // This makes it impossible to queue multiple downloads
    // by repeatedly pressing Accept.
    alert->m_button2->m_pfnSelector = [release, button](CCObject*) {
        if (s_downloading)
            return;

        s_downloading = true;

        if (button) {
            button->setEnabled(false);
            button->setOpacity(100);
        }

        // installRelease normally performs the lock itself,
        // so reset it here temporarily before handing control over.
        s_downloading = false;

        installRelease(release);
    };

    alert->show();
}

static void createReleaseButton(
    cocos2d::CCNode* parent,
    ReleaseInfo const& release,
    float x,
    float y,
    float buttonWidth,
    float labelScale,
    float buttonScale
) {
    auto label = CCLabelBMFont::create(
        release.name.empty()
            ? release.tag.c_str()
            : release.name.c_str(),
        "goldFont.fnt"
    );

    label->setScale(labelScale);
    label->setPosition({x, y});
    label->setAnchorPoint({0.5f, 0.5f});

    parent->addChild(label);

    auto button = CCMenuItemSpriteExtra::create(
        ButtonSprite::create(
            "Install",
            buttonWidth,
            true,
            "bigFont.fnt",
            "GJ_button_01.png",
            30.0f,
            buttonScale
        ),
        parent,
        [release](CCObject* sender) {
            if (s_downloading)
                return;

            auto button =
                static_cast<CCMenuItemSpriteExtra*>(sender);

            confirmInstall(
                release,
                button
            );
        }
    );

    button->setPosition({
        x,
        y - 17.f
    });

    static_cast<CCMenu*>(parent)->addChild(button);
}

void open() {
    auto currentVersion =
        Mod::get()->getVersion().toNonVString();

    github::get(
        RELEASES_URL,
        [currentVersion](geode::Result<github::WebResponse> result) {
            if (!result) {
                showError(
                    fmt::format(
                        "Failed to fetch releases.\n\n{}",
                        result.unwrapErr()
                    )
                );
                return;
            }

            auto jsonResult =
                result.unwrap().json();

            if (!jsonResult) {
                showError(
                    "Failed to parse GitHub response."
                );
                return;
            }

            auto json = jsonResult.unwrap();

            if (!json.isArray()) {
                showError(
                    "Invalid GitHub releases response."
                );
                return;
            }

            std::vector<ReleaseInfo> stable;
            std::vector<ReleaseInfo> dev;

            for (auto const& release : json) {
                auto tag =
                    release["tag_name"].asString();

                auto name =
                    release["name"].asString();

                auto prerelease =
                    release["prerelease"].asBool();

                auto draft =
                    release["draft"].asBool();

                if (!tag ||
                    !name ||
                    !prerelease ||
                    !draft) {
                    continue;
                }

                if (draft.unwrap())
                    continue;

                if (!isOlderVersion(
                    tag.unwrap(),
                    currentVersion
                )) {
                    continue;
                }

                auto assets =
                    release["assets"];

                if (!assets || !assets.isArray())
                    continue;

                std::string downloadURL;

                for (auto const& asset : assets) {
                    auto assetName =
                        asset["name"].asString();

                    auto assetURL =
                        asset["browser_download_url"].asString();

                    if (!assetName || !assetURL)
                        continue;

                    if (assetName.unwrap() == MOD_FILE) {
                        downloadURL =
                            assetURL.unwrap();
                        break;
                    }
                }

                if (downloadURL.empty())
                    continue;

                ReleaseInfo info;
                info.tag = tag.unwrap();
                info.name = name.unwrap();
                info.downloadURL = downloadURL;
                info.prerelease = prerelease.unwrap();

                if (info.prerelease)
                    dev.push_back(
                        std::move(info)
                    );
                else
                    stable.push_back(
                        std::move(info)
                    );
            }

            if (stable.empty() && dev.empty()) {
                FLAlertLayer::create(
                    "DLL Bot",
                    "No older releases found.",
                    "OK"
                )->show();

                return;
            }

#if defined(GEODE_IS_ANDROID) || defined(GEODE_IS_IOS)

            auto popup = CCScale9Sprite::create(
                "GJ_square01.png"
            );

            popup->setContentSize({
                300.f,
                220.f
            });

            popup->setPosition({
                150.f,
                110.f
            });

            auto title = CCLabelBMFont::create(
                "DLL Bot Downgrade",
                "goldFont.fnt"
            );

            title->setPosition({
                150.f,
                190.f
            });

            title->setScale(.52f);
            title->setAnchorPoint({
                .5f,
                .5f
            });

            popup->addChild(title);

            float stableX = 82.f;
            float devX = 218.f;
            float headerY = 160.f;
            float startY = 135.f;
            float spacing = 27.f;

            auto stableHeader = CCLabelBMFont::create(
                "Stable",
                "goldFont.fnt"
            );

            stableHeader->setPosition({
                stableX,
                headerY
            });

            stableHeader->setScale(.42f);
            stableHeader->setAnchorPoint({
                .5f,
                .5f
            });

            popup->addChild(stableHeader);

            auto devHeader = CCLabelBMFont::create(
                "Dev",
                "goldFont.fnt"
            );

            devHeader->setPosition({
                devX,
                headerY
            });

            devHeader->setScale(.42f);
            devHeader->setAnchorPoint({
                .5f,
                .5f
            });

            popup->addChild(devHeader);

            auto menu = CCMenu::create();
            menu->setPosition({
                0.f,
                0.f
            });

            popup->addChild(menu);

            for (size_t i = 0; i < stable.size(); ++i) {
                createReleaseButton(
                    menu,
                    stable[i],
                    stableX,
                    startY -
                        static_cast<float>(i) * spacing,
                    58.f,
                    .42f,
                    .42f
                );
            }

            for (size_t i = 0; i < dev.size(); ++i) {
                createReleaseButton(
                    menu,
                    dev[i],
                    devX,
                    startY -
                        static_cast<float>(i) * spacing,
                    58.f,
                    .42f,
                    .42f
                );
            }

#else

            auto popup = CCScale9Sprite::create(
                "GJ_square01.png"
            );

            popup->setContentSize({
                360.f,
                280.f
            });

            popup->setPosition({
                180.f,
                140.f
            });

            auto title = CCLabelBMFont::create(
                "DLL Bot Downgrade",
                "goldFont.fnt"
            );

            title->setPosition({
                180.f,
                245.f
            });

            title->setScale(.65f);
            title->setAnchorPoint({
                .5f,
                .5f
            });

            popup->addChild(title);

            float stableX = 100.f;
            float devX = 260.f;
            float headerY = 215.f;
            float startY = 180.f;
            float spacing = 32.f;

            auto stableHeader = CCLabelBMFont::create(
                "Stable",
                "goldFont.fnt"
            );

            stableHeader->setPosition({
                stableX,
                headerY
            });

            stableHeader->setScale(.5f);
            stableHeader->setAnchorPoint({
                .5f,
                .5f
            });

            popup->addChild(stableHeader);

            auto devHeader = CCLabelBMFont::create(
                "Dev",
                "goldFont.fnt"
            );

            devHeader->setPosition({
                devX,
                headerY
            });

            devHeader->setScale(.5f);
            devHeader->setAnchorPoint({
                .5f,
                .5f
            });

            popup->addChild(devHeader);

            auto menu = CCMenu::create();
            menu->setPosition({
                0.f,
                0.f
            });

            popup->addChild(menu);

            for (size_t i = 0; i < stable.size(); ++i) {
                createReleaseButton(
                    menu,
                    stable[i],
                    stableX,
                    startY -
                        static_cast<float>(i) * spacing,
                    70.f,
                    .5f,
                    .5f
                );
            }

            for (size_t i = 0; i < dev.size(); ++i) {
                createReleaseButton(
                    menu,
                    dev[i],
                    devX,
                    startY -
                        static_cast<float>(i) * spacing,
                    70.f,
                    .5f,
                    .5f
                );
            }

#endif

            auto scene =
                CCDirector::sharedDirector()
                    ->getRunningScene();

            if (scene) {
                scene->addChild(popup, 1000);
            }
        }
    );
}

}