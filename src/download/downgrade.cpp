#include "downgrade.hpp"
#include "github-api.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>

#include <array>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace downgrade {

static constexpr char const* RELEASES_URL =
    "https://api.github.com/repos/Homeless-Team/Dll-Bot/releases";

static constexpr char const* MOD_FILE =
    "homeless.dll-bot.geode";

static constexpr char const* TEMP_FILE =
    "DLL-Bot-downgrade.tmp.geode";

// Locked after Accept is pressed.
// This prevents multiple downloads / multiple .tmp files.
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

static void removeTempFile() {
    auto tempPath =
        Mod::get()->getSaveDir() /
        TEMP_FILE;

    std::error_code ec;

    if (std::filesystem::exists(tempPath, ec)) {
        std::filesystem::remove(tempPath, ec);
    }
}

static void installRelease(ReleaseInfo const& release) {
    if (!s_downloading)
        return;

    auto tempPath =
        Mod::get()->getSaveDir() /
        TEMP_FILE;

    github::download(
        release.downloadURL,
        [release, tempPath](geode::Result<geode::ByteVector> result) {
            if (!result) {
                removeTempFile();
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
                removeTempFile();
                resetDownloadLock();

                showError(
                    "Downloaded package is empty."
                );

                return;
            }

            // Write the new package to a temporary file first.
            if (!file::writeBinarySafe(tempPath, data)) {
                removeTempFile();
                resetDownloadLock();

                showError(
                    "Failed to write the downloaded package."
                );

                return;
            }

            std::error_code existsError;

            if (!std::filesystem::exists(tempPath, existsError)) {
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

            /*
             * Important:
             *
             * The download has already completed successfully.
             * Only now do we touch the currently installed DLL Bot.
             */

            // Remove the currently installed package.
            if (std::filesystem::exists(installedPath, existsError)) {
                std::error_code removeError;

                std::filesystem::remove(
                    installedPath,
                    removeError
                );

                if (removeError) {
                    removeTempFile();
                    resetDownloadLock();

                    showError(
                        fmt::format(
                            "Failed to remove the current DLL Bot.\n\n{}",
                            removeError.message()
                        )
                    );

                    return;
                }
            }

            // Remove an existing target package if it is different
            // from the currently installed package.
            std::error_code sameError;
            bool samePath =
                std::filesystem::equivalent(
                    installedPath,
                    targetPath,
                    sameError
                );

            if (!samePath &&
                std::filesystem::exists(targetPath, existsError)) {

                std::error_code removeError;

                std::filesystem::remove(
                    targetPath,
                    removeError
                );

                if (removeError) {
                    removeTempFile();
                    resetDownloadLock();

                    showError(
                        fmt::format(
                            "Failed to replace the DLL Bot package.\n\n{}",
                            removeError.message()
                        )
                    );

                    return;
                }
            }

            // Move the downloaded package into the mods directory.
            std::error_code renameError;

            std::filesystem::rename(
                tempPath,
                targetPath,
                renameError
            );

            if (renameError) {
                // Some filesystems may not support rename across
                // different filesystem boundaries, so try copy.
                std::error_code copyError;

                std::filesystem::copy_file(
                    tempPath,
                    targetPath,
                    std::filesystem::copy_options::overwrite_existing,
                    copyError
                );

                if (copyError) {
                    removeTempFile();
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

                std::filesystem::remove(
                    tempPath,
                    removeError
                );
            }

            /*
             * Installation succeeded.
             *
             * Keep s_downloading locked until Geometry Dash restarts.
             * This prevents the user from installing another version
             * into the same running game instance.
             */

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

/*
 * Confirmation popup
 */

class ConfirmDelegate : public FLAlertLayerProtocol {
protected:
    ReleaseInfo m_release;
    CCMenuItemSpriteExtra* m_button = nullptr;

public:
    static ConfirmDelegate* create(
        ReleaseInfo const& release,
        CCMenuItemSpriteExtra* button
    ) {
        auto ret = new ConfirmDelegate();

        ret->m_release = release;
        ret->m_button = button;

        if (ret) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    void FLAlert_Clicked(
        FLAlertLayer* layer,
        bool btn2
    ) override {
        if (!btn2)
            return;

        if (s_downloading)
            return;

        /*
         * Lock immediately when Accept is pressed.
         * The release button is also disabled immediately.
         */
        s_downloading = true;

        if (m_button) {
            m_button->setEnabled(false);
            m_button->setOpacity(100);
        }

        installRelease(m_release);
    }
};

static void confirmInstall(
    ReleaseInfo const& release,
    CCMenuItemSpriteExtra* button
) {
    if (s_downloading)
        return;

    auto message = fmt::format(
        "Are you sure you want to downgrade DLL Bot to {}?\n\n"
        "The current DLL Bot package will be replaced.\n"
        "Geometry Dash will need to be restarted after installation.",
        release.tag
    );

    auto delegate =
        ConfirmDelegate::create(
            release,
            button
        );

    if (!delegate)
        return;

    FLAlertLayer::create(
        delegate,
        "Confirm Downgrade",
        message.c_str(),
        "Cancel",
        "Accept"
    )->show();
}

/*
 * Release button callback target.
 *
 * CCMenuItemSpriteExtra requires a SEL_MenuHandler,
 * so we cannot pass a lambda directly.
 */

class ReleaseButtonDelegate : public CCObject {
protected:
    ReleaseInfo m_release;

public:
    static ReleaseButtonDelegate* create(
        ReleaseInfo const& release
    ) {
        auto ret = new ReleaseButtonDelegate();

        ret->m_release = release;

        if (ret) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    void onInstall(CCObject* sender) {
        if (s_downloading)
            return;

        auto button =
            static_cast<CCMenuItemSpriteExtra*>(sender);

        confirmInstall(
            m_release,
            button
        );
    }
};

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
    label->setPosition({
        x,
        y
    });
    label->setAnchorPoint({
        0.5f,
        0.5f
    });

    parent->addChild(label);

    auto delegate =
        ReleaseButtonDelegate::create(release);

    if (!delegate)
        return;

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
        delegate,
        menu_selector(
            ReleaseButtonDelegate::onInstall
        )
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
        [currentVersion](
            geode::Result<github::WebResponse> result
        ) {
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

            auto json =
                jsonResult.unwrap();

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

                /*
                 * Result<T> is checked for errors here.
                 *
                 * Do NOT use:
                 *
                 * !prerelease
                 *
                 * because prerelease=false is a valid result.
                 */

                if (!tag ||
                    !name ||
                    !prerelease ||
                    !draft) {
                    continue;
                }

                if (draft.unwrap())
                    continue;

                auto tagValue =
                    tag.unwrap();

                if (!isOlderVersion(
                    tagValue,
                    currentVersion
                )) {
                    continue;
                }

                auto assets =
                    release["assets"];

                if (!assets.isArray())
                    continue;

                std::string downloadURL;

                for (auto const& asset : assets) {
                    auto assetName =
                        asset["name"].asString();

                    auto assetURL =
                        asset["browser_download_url"].asString();

                    if (!assetName ||
                        !assetURL) {
                        continue;
                    }

                    if (assetName.unwrap() == MOD_FILE) {
                        downloadURL =
                            assetURL.unwrap();

                        break;
                    }
                }

                if (downloadURL.empty())
                    continue;

                ReleaseInfo info;

                info.tag =
                    std::move(tagValue);

                info.name =
                    name.unwrap();

                info.downloadURL =
                    std::move(downloadURL);

                info.prerelease =
                    prerelease.unwrap();

                if (info.prerelease) {
                    dev.push_back(
                        std::move(info)
                    );
                }
                else {
                    stable.push_back(
                        std::move(info)
                    );
                }
            }

            if (stable.empty() &&
                dev.empty()) {

                FLAlertLayer::create(
                    "DLL Bot",
                    "No older releases found.",
                    "OK"
                )->show();

                return;
            }

#if defined(GEODE_IS_ANDROID) || defined(GEODE_IS_IOS)

            /*
             * Mobile UI
             */

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

            /*
             * PC UI
             */

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
                scene->addChild(
                    popup,
                    1000
                );
            }
        }
    );
}

}