#pragma once
#include "../includes.hpp"

class SaveMacroLayer : public geode::Popup {

    TextInput* authorInput = nullptr;
    TextInput* descInput = nullptr;
    TextInput* nameInput = nullptr;

    // 3 format cycle qua nút bấm, thay vì 2 checkbox độc lập (dễ gây nhầm
    // vì trạng thái "cả 2 tắt" = .gdr không có label nào hiển thị).
    enum class Format { Gdr, GdrJson, Gdr2 };
    Format currentFormat = Format::Gdr;

    CCMenuItemSpriteExtra* formatBtn = nullptr;
    CCLabelBMFont* formatLbl = nullptr;

private:

    std::string formatLabel() const {
        switch (currentFormat) {
            case Format::Gdr:     return "Format: GDR";
            case Format::GdrJson: return "Format: JSON";
            case Format::Gdr2:    return "Format: GDR2";
        }
        return "Format: GDR";
    }

    bool setup() {
        // Utils::setBackgroundColor(m_bgSprite);

        setTitle("Save Macro");

        cocos2d::CCPoint offset = (CCDirector::sharedDirector()->getWinSize() - m_mainLayer->getContentSize()) / 2;
        m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);
        m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);
        m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);
        m_title->setPosition(m_title->getPosition() + offset);

        CCMenu* menu = CCMenu::create();
        m_mainLayer->addChild(menu);

        authorInput = TextInput::create(104, "Author", "chatFont.fnt");
        authorInput->setPosition({ 61, 42 });
        authorInput->setString(GJAccountManager::sharedState()->m_username.c_str());
        menu->addChild(authorInput);

        CCLabelBMFont* lbl = CCLabelBMFont::create("(optional)", "chatFont.fnt");
        lbl->setPosition({ 61, 20 });
        lbl->setOpacity(73);
        lbl->setScale(0.575);
        menu->addChild(lbl);

        nameInput = TextInput::create(104, "Name", "chatFont.fnt");
        nameInput->setPosition({ -61, 42 });

        nameInput->setString(Global::get().macro.levelInfo.name);

        menu->addChild(nameInput);

        descInput = TextInput::create(226, "Description (optional)", "chatFont.fnt");
        descInput->setPositionY(-8);
        menu->addChild(descInput);

        ButtonSprite* spr = ButtonSprite::create("Save");
        spr->setScale(0.725f);
        CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(SaveMacroLayer::onSave));
        btn->setPositionY(-56);
        menu->addChild(btn);

        // Nút cycle format — bấm để chuyển GDR -> JSON -> GDR2 -> GDR...
        ButtonSprite* formatSpr = ButtonSprite::create(formatLabel().c_str(), 90, true, "bigFont.fnt", "GJ_button_04.png", 0, 0.6f);
        formatSpr->setScale(0.6f);
        formatBtn = CCMenuItemSpriteExtra::create(formatSpr, this, menu_selector(SaveMacroLayer::onCycleFormat));
        formatBtn->setPosition({ -83, -78 });
        menu->addChild(formatBtn);

        return true;
    }

public:

    STATIC_CREATE(SaveMacroLayer, 285, 194)
    
    static void open() {
        if (Global::get().macro.inputs.empty())
            return FLAlertLayer::create("Save Macro", "You can't save an <cl>empty</c> macro.", "Ok")->show();

        std::filesystem::path path = Mod::get()->getSettingValue<std::filesystem::path>("macros_folder");

        if (!std::filesystem::exists(path)) {
            if (!utils::file::createDirectoryAll(path).isOk())
                return FLAlertLayer::create("Error", ("There was an error getting the folder \"" + path.string() + "\". ID: 10").c_str(), "Ok")->show();
        }

        SaveMacroLayer* layerReal = create();
        layerReal->m_noElasticity = true;
        layerReal->show();
    }

    void onCycleFormat(CCObject*) {
        switch (currentFormat) {
            case Format::Gdr:     currentFormat = Format::GdrJson; break;
            case Format::GdrJson: currentFormat = Format::Gdr2; break;
            case Format::Gdr2:    currentFormat = Format::Gdr; break;
        }

        // Đổi lại label trên nút để phản ánh format vừa chọn.
        if (CCObject* obj = formatBtn->getChildren()->objectAtIndex(0)) {
            if (ButtonSprite* bs = typeinfo_cast<ButtonSprite*>(obj))
                bs->setString(formatLabel().c_str());
        }
    }

    void onSave(CCObject*) {
        std::string macroName = nameInput->getString();
        if (macroName == "")
            return FLAlertLayer::create("Save Macro", "Give a <cl>name</c> to the macro.", "Ok")->show();

        std::filesystem::path path = Mod::get()->getSettingValue<std::filesystem::path>("macros_folder") / macroName;
        std::string author = authorInput->getString();
        std::string desc = descInput->getString();

        int result = 0;
        switch (currentFormat) {
            case Format::Gdr:
                result = Macro::save(author, desc, path.string(), false);
                break;
            case Format::GdrJson:
                result = Macro::save(author, desc, path.string(), true);
                break;
            case Format::Gdr2:
                result = Macro::saveGDR2(author, desc, path.string());
                break;
        }

        if (result != 0)
            return FLAlertLayer::create("Error", "There was an error saving the macro. ID: " + std::to_string(result), "Ok")->show();

        this->keyBackClicked();
        Notification::create("Macro Saved", NotificationIcon::Success)->show();
    }

};
