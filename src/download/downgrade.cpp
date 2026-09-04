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

struct ReleaseInfo {
    std::string tag;
    std::string name;
    std::string downloadURL;
    bool prerelease = false;
};

static std::array<int, 3> parseVersion(std::string version) {
    // Remove leading v/V
    if (!version.empty() &&
        (version[0] == 'v' || version[0] == 'V')) {
        version.erase(0, 1);
    }

    // Remove prerelease suffix:
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

static void installRelease(ReleaseInfo const& release) {
    github::download(
        release.downloadURL,
        [release](geode::Result<geode::ByteVector> result) {
            if (!result) {
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
                showError("Downloaded file is empty.");
                return;
            }

            auto tempPath =
                Mod::get()->getSaveDir() /
                "DLL-Bot-downgrade.tmp.geode";

            auto targetPath =
                dirs::getModsDir() /
                MOD_FILE;

            auto installedPath =
                Mod::get()->getPackagePath();

            // Never touch the current installation until
            // the new package has been downloaded successfully.
            if (!file::writeBinarySafe(tempPath, data)) {
                showError(
                    "Failed to write the downloaded package."
                );
                return;
            }

            // Make sure the temporary package actually exists.
            if (!ghc::filesystem::exists(tempPath)) {
                showError(
                    "Downloaded package could not be created."
                );
                return;
            }

            // Remove the currently installed DLL Bot package.
            if (ghc::filesystem::exists(installedPath)) {
                std::error_code ec;
                ghc::filesystem::remove(installedPath, ec);

                if (ec) {
                    ghc::filesystem::remove(tempPath);

                    showError(
                        fmt::format(
                            "Failed to remove the current DLL Bot.\n\n{}",
                            ec.message()
                        )
                    );
                    return;
                }
            }

            // Remove the target package if it is still present.
            if (ghc::filesystem::exists(targetPath)) {
                std::error_code ec;
                ghc::filesystem::remove(targetPath, ec);

                if (ec) {
                    ghc::filesystem::remove(tempPath);

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
                // Fallback: copy + remove temp if rename fails
                // because of filesystem restrictions.
                std::error_code copyError;

                ghc::filesystem::copy_file(
                    tempPath,
                    targetPath,
                    ghc::filesystem::copy_options::overwrite_existing,
                    copyError
                );

                if (copyError) {
                    ghc::filesystem::remove(tempPath);

                    showError(
                        fmt::format(
                            "Failed to install DLL Bot {}.\n\n{}",
                            release.tag,
                            copyError.message()
                        )
                    );
                    return;
                }

                ghc::filesystem::remove(tempPath);
            }

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
        [release](CCObject*) {
            installRelease(release);
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
                showError("Failed to parse GitHub response.");
                return;
            }

            auto json = jsonResult.unwrap();

            if (!json.isArray()) {
                showError("Invalid GitHub releases response.");
                return;
            }

            std::vector<ReleaseInfo> stable;
            std::vector<ReleaseInfo> dev;

            for (auto const& release : json) {
                auto tag = release["tag_name"].asString();
                auto name = release["name"].asString();
                auto prerelease = release["prerelease"].asBool();
                auto draft = release["draft"].asBool();

                if (!tag || !name || !prerelease || !draft)
                    continue;

                // Never show GitHub drafts.
                if (draft.unwrap())
                    continue;

                // Only show versions older than the currently
                // installed version.
                if (!isOlderVersion(
                    tag.unwrap(),
                    currentVersion
                )) {
                    continue;
                }

                auto assets = release["assets"];

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
                        downloadURL = assetURL.unwrap();
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
                    dev.push_back(std::move(info));
                else
                    stable.push_back(std::move(info));
            }

            if (stable.empty() && dev.empty()) {
                FLAlertLayer::create(
                    "DLL Bot",
                    "No older releases found.",
                    "OK"
                )->show();

                return;
            }

            // -------------------------------------------------
            // UI
            // -------------------------------------------------

            auto layer = CCLayerColor::create(
                {0, 0, 0, 0}
            );

            auto popup = CCScale9Sprite::create(
                "GJ_square01.png"
            );

#if defined(GEODE_IS_ANDROID) || defined(GEODE_IS_IOS)

            popup->setContentSize({300.f, 220.f});
            popup->setPosition({150.f, 110.f});

            auto title = CCLabelBMFont::create(
                "DLL Bot Downgrade",
                "goldFont.fnt"
            );

            title->setPosition({150.f, 190.f});
            title->setScale(.52f);
            title->setAnchorPoint({0.5f, 0.5f});

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
            stableHeader->setAnchorPoint({0.5f, 0.5f});

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
            devHeader->setAnchorPoint({0.5f, 0.5f});

            popup->addChild(devHeader);

            auto menu = CCMenu::create();
            menu->setPosition({0, 0});
            popup->addChild(menu);

            for (size_t i = 0; i < stable.size(); ++i) {
                createReleaseButton(
                    menu,
                    stable[i],
                    stableX,
                    startY - static_cast<float>(i) * spacing,
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
                    startY - static_cast<float>(i) * spacing,
                    58.f,
                    .42f,
                    .42f
                );
            }

#else

            popup->setContentSize({360.f, 280.f});
            popup->setPosition({180.f, 140.f});

            auto title = CCLabelBMFont::create(
                "DLL Bot Downgrade",
                "goldFont.fnt"
            );

            title->setPosition({180.f, 245.f});
            title->setScale(.65f);
            title->setAnchorPoint({0.5f, 0.5f});

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
            stableHeader->setAnchorPoint({0.5f, 0.5f});

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
            devHeader->setAnchorPoint({0.5f, 0.5f});

            popup->addChild(devHeader);

            auto menu = CCMenu::create();
            menu->setPosition({0, 0});
            popup->addChild(menu);

            for (size_t i = 0; i < stable.size(); ++i) {
                createReleaseButton(
                    menu,
                    stable[i],
                    stableX,
                    startY - static_cast<float>(i) * spacing,
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
                    startY - static_cast<float>(i) * spacing,
                    70.f,
                    .5f,
                    .5f
                );
            }

#endif

            auto closeMenu = CCMenu::create();
            closeMenu->setPosition({0, 0});
            popup->addChild(closeMenu);

            auto closeButton = CCMenuItemSpriteExtra::create(
                CCSprite::createWithSpriteFrameName(
                    "GJ_closeBtn_001.png"
                ),
                popup,
                [](CCObject*) {
                    if (auto scene = CCDirector::sharedDirector()
                        ->getRunningScene()) {
                        // The popup is removed by its parent layer.
                    }
                }
            );

#if defined(GEODE_IS_ANDROID) || defined(GEODE_IS_IOS)
            closeButton->setPosition({286.f, 204.f});
#else
            closeButton->setPosition({346.f, 264.f});
#endif

            closeMenu->addChild(closeButton);

            auto alert = FLAlertLayer::create(
                "DLL Bot",
                "Select a release to install.",
                "Close"
            );

            alert->m_scene = layer;

            popup->addChild(layer);
            layer->addChild(popup);

            alert->show();
        }
    );
}

}