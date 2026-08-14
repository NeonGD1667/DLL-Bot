#pragma once

#include "../includes.hpp"

class RGBEffect : public cocos2d::CCNode {
private:
    std::vector<cocos2d::CCNode*> m_targets;

    float m_speed = 60.f;
    float m_hue = 0.f;
    float m_saturation = 1.f;
    float m_value = 1.f;

    static cocos2d::ccColor3B hsvToRgb(float h, float s, float v) {
        h = std::fmod(h, 360.f);

        if (h < 0.f)
            h += 360.f;

        const float c = v * s;
        const float x =
            c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
        const float m = v - c;

        float r = 0.f;
        float g = 0.f;
        float b = 0.f;

        if (h < 60.f) {
            r = c;
            g = x;
        }
        else if (h < 120.f) {
            r = x;
            g = c;
        }
        else if (h < 180.f) {
            g = c;
            b = x;
        }
        else if (h < 240.f) {
            g = x;
            b = c;
        }
        else if (h < 300.f) {
            r = x;
            b = c;
        }
        else {
            r = c;
            b = x;
        }

        return cocos2d::ccc3(
            static_cast<GLubyte>((r + m) * 255.f),
            static_cast<GLubyte>((g + m) * 255.f),
            static_cast<GLubyte>((b + m) * 255.f)
        );
    }

    static void applyColor(cocos2d::CCNode* node, cocos2d::ccColor3B color) {
        if (!node)
            return;

        if (auto* sprite = typeinfo_cast<cocos2d::CCSprite*>(node)) {
            sprite->setColor(color);
            return;
        }

        if (auto* label = typeinfo_cast<cocos2d::CCLabelBMFont*>(node)) {
            label->setColor(color);
            return;
        }

        if (auto* scale9 =
                typeinfo_cast<cocos2d::extension::CCScale9Sprite*>(node)) {
            scale9->setColor(color);
            return;
        }

        // CCNode không có setColor().
        // Những loại node khác sẽ bị bỏ qua.
    }

public:
    static RGBEffect* create(
        std::vector<cocos2d::CCNode*> const& targets,
        float speed = 60.f,
        float saturation = 1.f,
        float value = 1.f
    ) {
        auto* ret = new RGBEffect();

        if (!ret->init()) {
            delete ret;
            return nullptr;
        }

        ret->m_targets = targets;
        ret->m_speed = speed;
        ret->m_saturation = saturation;
        ret->m_value = value;

        ret->scheduleUpdate();
        ret->autorelease();

        return ret;
    }

    void addTarget(cocos2d::CCNode* target) {
        if (!target)
            return;

        m_targets.push_back(target);
    }

    void removeTarget(cocos2d::CCNode* target) {
        if (!target)
            return;

        std::erase(m_targets, target);
    }

    void clearTargets() {
        m_targets.clear();
    }

    void setSpeed(float speed) {
        m_speed = speed;
    }

    float getSpeed() const {
        return m_speed;
    }

    void update(float dt) override {
        m_hue += m_speed * dt;

        while (m_hue >= 360.f)
            m_hue -= 360.f;

        while (m_hue < 0.f)
            m_hue += 360.f;

        auto color = hsvToRgb(
            m_hue,
            m_saturation,
            m_value
        );

        for (auto* node : m_targets) {
            applyColor(node, color);
        }
    }
};
