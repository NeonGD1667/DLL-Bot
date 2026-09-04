#include "downgrade.hpp"
#include "github-api.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

namespace downgrade {

static constexpr char const* API_URL =
    "https://api.github.com/repos/Homeless-Team/Dll-Bot/releases";

struct Release {
    std::string tag;
    std::string name;
    bool dev;
    std::string downloadURL;
};

static std::vector<Release> s_releases;
static bool s_loading = false;

static void downloadRelease(Release const& release);

class DowngradePopup : public Popup {
protected:
    bool init() {
#ifdef GEODE_IS_ANDROID
        if (!Popup::init(300.f, 220.f))
            return false;

        auto title = CCLabelBMFont::create(
            "DLL Bot Downgrade",
            "bigFont.fnt"
        );

        title->setPosition({150.f, 190.f});
        title->setScale(.52f);
        addChild(title);

        createReleaseUI();
#else
        if (!Popup::init(360.f, 280.f))
            return false;

        auto title = CCLabelBMFont::create(
            "DLL Bot Downgrade",
            "bigFont.fnt"
        );

        title->setPosition({180.f, 245.f});
        title->setScale(.65f);
        addChild(title);

        createReleaseUI();
#endif

        return true;
    }

    void createReleaseUI() {

#ifdef GEODE_IS_ANDROID
        constexpr float centerX = 150.f;
        constexpr float stableX = 82.f;
        constexpr float devX = 218.f;
        constexpr float headerY = 160.f;
        constexpr float startY = 135.f;
        constexpr float spacing = 27.f;
        constexpr float labelScale = .42f;
        constexpr float buttonScale = .42f;
        constexpr int buttonWidth = 58;
#else
        constexpr float centerX = 180.f;
        constexpr float stableX = 100.f;
        constexpr float devX = 260.f;
        constexpr float headerY = 215.f;
        constexpr float startY = 180.f;
        constexpr float spacing = 32.f;
        constexpr float labelScale = .5f;
        constexpr float buttonScale = .5f;
        constexpr int buttonWidth = 70;
#endif

        auto stable = CCLabelBMFont::create(
            "Stable",
            "goldFont.fnt"
        );

        stable->setPosition({stableX, headerY});
        stable->setScale(labelScale);
        addChild(stable);

        auto dev = CCLabelBMFont::create(
            "Dev",
            "goldFont.fnt"
        );

        dev->setPosition({devX, headerY});
        dev->setScale(labelScale);
        addChild(dev);

        auto stableMenu = CCMenu::create();
        stableMenu->setPosition({0.f, 0.f});
        addChild(stableMenu);

        auto devMenu = CCMenu::create();
        devMenu->setPosition({0.f, 0.f});
        addChild(devMenu);

        float stableY = startY;
        float devY = startY;

        for (int i = 0; i < static_cast<int>(s_releases.size()); ++i) {
            auto const& release = s_releases[i];

            auto button = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(
                    release.tag.c_str(),
                    buttonWidth,
                    true,
                    "bigFont.fnt",
                    "GJ_button_01.png",
                    25.f,
                    buttonScale
                ),
                this,
                menu_selector(DowngradePopup::onRelease)
            );

            button->setTag(i);

            if (release.dev) {
                button->setPosition({devX, devY});
                devMenu->addChild(button);
                devY -= spacing;
            }
            else {
                button->setPosition({stableX, stableY});
                stableMenu->addChild(button);
                stableY -= spacing;
            }
        }

        if (s_releases.empty()) {
            auto label = CCLabelBMFont::create(
                "No releases found.",
                "bigFont.fnt"
            );

            label->setPosition({centerX, 100.f});
            label->setScale(labelScale);
            addChild(label);
        }
    }

    void onRelease(CCObject* sender) {
        auto button = static_cast<CCNode*>(sender);
        auto index = button->getTag();

        if (
            index < 0 ||
            index >= static_cast<int>(s_releases.size())
        )
            return;

        downloadRelease(s_releases[index]);
    }

public:
    static DowngradePopup* create() {
        auto popup = new DowngradePopup;

        if (popup->init()) {
            popup->autorelease();
            return popup;
        }

        delete popup;
        return nullptr;
    }
};

static void downloadRelease(Release const& release) {
    if (release.downloadURL.empty()) {
        FLAlertLayer::create(
            "DLL Bot",
            "This release has no <cj>.geode</c> asset.",
            "OK"
        )->show();

        return;
    }

    github::download(
        release.downloadURL,
        [release](Result<ByteVector> result) {
            if (!result) {
                FLAlertLayer::create(
                    "Download Failed",
                    result.unwrapErr().c_str(),
                    "OK"
                )->show();

                return;
            }

            auto const& data = result.unwrap();

            auto path =
                Mod::get()->getSaveDir() /
                fmt::format(
                    "DLL-Bot-{}.geode",
                    release.tag
                );

            auto written = file::writeBinarySafe(path, data);

            if (!written) {
                FLAlertLayer::create(
                    "Download Failed",
                    written.unwrapErr().c_str(),
                    "OK"
                )->show();

                return;
            }

            FLAlertLayer::create(
                "Download Complete",
                fmt::format(
                    "Downloaded {} successfully.",
                    release.tag
                ).c_str(),
                "OK"
            )->show();
        }
    );
}

static void fetchReleases() {
    if (s_loading)
        return;

    s_loading = true;

    github::get(
        API_URL,
        [](Result<github::WebResponse> result) {
            s_loading = false;

            if (!result) {
                FLAlertLayer::create(
                    "DLL Bot",
                    result.unwrapErr().c_str(),
                    "OK"
                )->show();

                return;
            }

            auto json = result.unwrap().json();

            if (!json) {
                FLAlertLayer::create(
                    "DLL Bot",
                    "Failed to parse GitHub response.",
                    "OK"
                )->show();

                return;
            }

            auto const& releases = json.unwrap();

            if (!releases.isArray()) {
                FLAlertLayer::create(
                    "DLL Bot",
                    "Invalid GitHub releases response.",
                    "OK"
                )->show();

                return;
            }

            s_releases.clear();

            for (auto const& release : releases) {
                auto tag = release["tag_name"].asString();
                auto name = release["name"].asString();
                auto prerelease = release["prerelease"].asBool();
                auto draft = release["draft"].asBool();
                auto assets = release["assets"];

                if (!tag || !name || !prerelease || !draft)
                    continue;

                if (draft.unwrap())
                    continue;

                std::string downloadURL;

                if (assets.isArray()) {
                    for (auto const& asset : assets) {
                        auto assetName =
                            asset["name"].asString();

                        auto browserURL =
                            asset["browser_download_url"].asString();

                        if (!assetName || !browserURL)
                            continue;

                        auto filename = assetName.unwrap();

                        if (
                            filename.size() >= 6 &&
                            filename.ends_with(".geode")
                        ) {
                            downloadURL = browserURL.unwrap();
                            break;
                        }
                    }
                }

                s_releases.push_back({
                    tag.unwrap(),
                    name.unwrap(),
                    prerelease.unwrap(),
                    downloadURL
                });
            }

            if (auto popup = DowngradePopup::create())
                popup->show();
        }
    );
}

void open() {
    fetchReleases();
}

}