#pragma once

#include <Geode/loader/SettingV3.hpp>

class VersionManagerSettingV3 : public geode::SettingV3 {
public:
    static geode::Result<std::shared_ptr<geode::SettingV3>> parse(
        std::string const& key,
        std::string const& modID,
        matjson::Value const& json
    );

    bool load(matjson::Value const& json) override;
    bool save(matjson::Value& json) const override;

    bool isDefaultValue() const override;
    void reset() override;

    geode::SettingNodeV3* createNode(float width) override;
};

class VersionManagerSettingNodeV3 : public geode::SettingNodeV3 {
protected:
    bool init(
        std::shared_ptr<VersionManagerSettingV3> setting,
        float width
    );

    void updateState(CCNode* invoker) override;

    void onCheckUpdate(CCObject*);
    void onDowngrade(CCObject*);

    void onCommit() override;
    void onResetToDefault() override;

public:
    static VersionManagerSettingNodeV3* create(
        std::shared_ptr<VersionManagerSettingV3> setting,
        float width
    );

    bool hasUncommittedChanges() const override;
    bool hasNonDefaultValue() const override;

    std::shared_ptr<VersionManagerSettingV3> getSetting() const;
};