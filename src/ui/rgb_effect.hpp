#pragma once
#include "../includes.hpp"

// Node tiện ích: chạy 1 vòng lặp update, đổi màu dần theo bánh xe HSV (hue
// quay đều 360 độ) rồi áp màu đó lên toàn bộ target node truyền vào.
// Add làm con của bất kỳ layer nào cần hiệu ứng RGB.
//
// Cách dùng:
//   RGBEffect* fx = RGBEffect::create({ topBorder, bottomBorder, leftBorder, rightBorder });
//   this->addChild(fx);
//
// Muốn tắt hiệu ứng: fx->removeFromParentAndCleanup(true);
// (các target sẽ giữ nguyên màu cuối cùng nó tint, gọi lại setColor thủ công
// nếu muốn reset về màu tĩnh ban đầu)

class RGBEffect : public cocos2d::CCNode {
private:
    std::vector<cocos2d::CCNode*> targets;
    float speed = 60.f; // độ/giây trên bánh xe hue (60 = full vòng trong 6s)
    float hue = 0.f;
    float saturation = 1.f;
    float value = 1.f;

    static cocos2d::ccColor3B hsvToRgb(float h, float s, float v) {
        h = std::fmod(h, 360.f);
        if (h < 0.f) h += 360.f;

        float c = v * s;
        float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
        float m = v - c;

        float r = 0.f, g = 0.f, b = 0.f;
        if (h < 60.f)       { r = c; g = x; b = 0.f; }
        else if (h < 120.f) { r = x; g = c; b = 0.f; }
        else if (h < 180.f) { r = 0.f; g = c; b = x; }
        else if (h < 240.f) { r = 0.f; g = x; b = c; }
        else if (h < 300.f) { r = x; g = 0.f; b = c; }
        else                { r = c; g = 0.f; b = x; }

        return cocos2d::ccc3(
            static_cast<GLubyte>((r + m) * 255.f),
            static_cast<GLubyte>((g + m) * 255.f),
            static_cast<GLubyte>((b + m) * 255.f)
        );
    }

public:
    // targets: danh sách node sẽ bị đổi màu mỗi frame
    // speed: tốc độ quay hue, độ/giây (mặc định 60 -> full chu kỳ màu trong 6s)
    // saturation/value: độ bão hòa/độ sáng cố định, mặc định full màu tươi
    static RGBEffect* create(std::vector<cocos2d::CCNode*> const& targets,
                             float speed = 60.f, float saturation = 1.f, float value = 1.f) {
        RGBEffect* ret = new RGBEffect();
        if (ret->init()) {
            ret->targets = targets;
            ret->speed = speed;
            ret->saturation = saturation;
            ret->value = value;
            ret->scheduleUpdate();
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    // Thêm target sau khi đã tạo (VD node được tạo muộn hơn trong setup()).
    void addTarget(cocos2d::CCNode* target) {
        if (target) targets.push_back(target);
    }

    void update(float dt) override {
        hue += speed * dt;
        if (hue >= 360.f) hue -= 360.f;

        cocos2d::ccColor3B color = hsvToRgb(hue, saturation, value);

        for (cocos2d::CCNode* node : targets) {
            if (!node) continue;

            if (auto* sprite = typeinfo_cast<cocos2d::CCSprite*>(node))
                sprite->setColor(color);
            else if (auto* label = typeinfo_cast<cocos2d::CCLabelBMFont*>(node))
                label->setColor(color);
            else if (auto* scale9 = typeinfo_cast<cocos2d::extension::CCScale9Sprite*>(node))
                scale9->setColor(color);
            else
                node->setColor(color); // fallback, CCNode có setColor ảo
        }
    }
};
