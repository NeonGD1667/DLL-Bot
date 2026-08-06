#pragma once

#include "../includes.hpp"

class StepShiftSettingsLayer : public geode::Popup {
protected:
  std::string m_configKey;
  static inline std::string s_nextConfigKey = "macro_step_shift_size";

  bool setup() {
    m_configKey = s_nextConfigKey;
    setTitle(m_configKey == "macro_step_shift_size" ? "Custom Step Size"
                                                    : "Hold Step Size");

    cocos2d::CCPoint offset = (CCDirector::sharedDirector()->getWinSize() -
                               m_mainLayer->getContentSize()) /
                              2;
    m_mainLayer->setPosition(m_mainLayer->getPosition() - offset);
    m_closeBtn->setPosition(m_closeBtn->getPosition() + offset);
    m_bgSprite->setPosition(m_bgSprite->getPosition() + offset);
    m_title->setPosition(m_title->getPosition() + offset);

    cocos2d::CCMenu *menu = cocos2d::CCMenu::create();
    menu->setID("step-shift-settings-menu");
    m_mainLayer->addChild(menu);

    cocos2d::CCPoint center = cocos2d::CCPoint{110, 75} + offset;

    auto inputBg = cocos2d::extension::CCScale9Sprite::create(
        "square02_small.png", {0, 0, 40, 40});
    inputBg->setOpacity(120);
    inputBg->setContentSize({60, 30});
    inputBg->setPosition(center);
    m_mainLayer->addChild(inputBg);

    auto input = CCTextInputNode::create(50, 20, "10", "bigFont.fnt");
    input->setAllowedChars("0123456789");
    input->setMaxLabelScale(0.7f);
    input->setMaxLabelLength(3);
    input->setString(
        std::to_string(Global::get().mod->getSavedValue<int64_t>(m_configKey))
            .c_str());
    input->setPosition(center);
    input->setID("step-shift-input");
    m_mainLayer->addChild(input);

    return true;
  }

  void onClose(cocos2d::CCObject *sender) {
    if (auto input = typeinfo_cast<CCTextInputNode *>(
            m_mainLayer->getChildByID("step-shift-input"))) {
      if (std::string(input->getString()) != "") {
        Global::get().mod->setSavedValue(
            m_configKey, static_cast<int64_t>(std::stoi(input->getString())));
      }
    }
    geode::Popup::onClose(sender);
  }

public:
  STATIC_CREATE(StepShiftSettingsLayer, 220, 150)

  static void openShift(cocos2d::CCObject *) {
    s_nextConfigKey = "macro_step_shift_size";
    StepShiftSettingsLayer::create()->show();
  }

  static void openHold(cocos2d::CCObject *) {
    s_nextConfigKey = "macro_hold_step_size";
    StepShiftSettingsLayer::create()->show();
  }
};
