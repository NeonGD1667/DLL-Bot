#include "version_manager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/Mod.hpp>

using namespace geode::prelude;

namespace {

void showComingSoon(char const* title) {
    FLAlertLayer::create(
        title,
        "Version manager is not implemented yet.",
        "OK"
    )->show();
}

}

Result<std::shared_ptr<SettingV3>> VersionManagerSettingV3::parse(
    std::string const& key,
    std::string const& modID,
    matjson::Value const& json
) {
    auto res = std::make_shared<VersionManagerSettingV3>();

    auto root = checkJson(
        json,
        "VersionManagerSettingV3"
    );

    res->init(
        key,
        modID,
        root
    );

    res->parseNameAndDescription(root);
    res->parseEnableIf(root);

    root.checkUnknownKeys();

    return root.ok(
        std::static_pointer_cast<SettingV3>(res)
    );
}

bool VersionManagerSettingV3::load(matjson::Value const&) {
    return true;
}

bool VersionManagerSettingV3::save(matjson::Value&) const {
    return true;
}

bool VersionManagerSettingV3::isDefaultValue() const {
    return true;
}

void VersionManagerSettingV3::reset() {}

SettingNodeV3* VersionManagerSettingV3::createNode(float width) {
    return VersionManagerSettingNodeV3::create(
        std::static_pointer_cast<VersionManagerSettingV3>(
            shared_from_this()
        ),
        width
    );
}

bool VersionManagerSettingNodeV3::init(
    std::shared_ptr<VersionManagerSettingV3> setting,
    float width
) {
    if (!SettingNodeV3::init(setting, width))
        return false;

    auto menu = getButtonMenu();

    /*
     * Check for updates
     */
    auto checkSprite =
        CCSprite::createWithSpriteFrameName(
            "GJ_updateBtn_001.png"
        );

    if (!checkSprite)
        return false;

    checkSprite->setScale(0.45f);

    auto checkButton = CCMenuItemSpriteExtra::create(
        checkSprite,
        this,
        menu_selector(
            VersionManagerSettingNodeV3::onCheckUpdate
        )
    );

    menu->addChild(checkButton);

    /*
     * Downgrade
     */
    auto downgradeSprite =
        CCSprite::createWithSpriteFrameName(
            "GJ_downloadBtn_001.png"
        );

    if (!downgradeSprite)
        return false;

    downgradeSprite->setScale(0.45f);

    auto downgradeButton = CCMenuItemSpriteExtra::create(
        downgradeSprite,
        this,
        menu_selector(
            VersionManagerSettingNodeV3::onDowngrade
        )
    );

    menu->addChild(downgradeButton);

    menu->updateLayout();

    updateState(nullptr);

    return true;
}

void VersionManagerSettingNodeV3::updateState(
    CCNode* invoker
) {
    SettingNodeV3::updateState(invoker);
}

void VersionManagerSettingNodeV3::onCheckUpdate(
    CCObject*
) {
    showComingSoon("Check for Updates");
}

void VersionManagerSettingNodeV3::onDowngrade(
    CCObject*
) {
    showComingSoon("Downgrade");
}

void VersionManagerSettingNodeV3::onCommit() {}

void VersionManagerSettingNodeV3::onResetToDefault() {}

VersionManagerSettingNodeV3*
VersionManagerSettingNodeV3::create(
    std::shared_ptr<VersionManagerSettingV3> setting,
    float width
) {
    auto ret = new VersionManagerSettingNodeV3();

    if (
        ret &&
        ret->init(setting, width)
    ) {
        ret->autorelease();
        return ret;
    }

    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool VersionManagerSettingNodeV3::hasUncommittedChanges() const {
    return false;
}

bool VersionManagerSettingNodeV3::hasNonDefaultValue() const {
    return false;
}

std::shared_ptr<VersionManagerSettingV3>
VersionManagerSettingNodeV3::getSetting() const {
    return std::static_pointer_cast<VersionManagerSettingV3>(
        SettingNodeV3::getSetting()
    );
}

$execute {
    (void)Mod::get()->registerCustomSettingType(
        "version-manager",
        &VersionManagerSettingV3::parse
    );
}