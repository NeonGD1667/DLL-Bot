// ===== Thêm include vào đầu load_macro_layer.cpp =====
#include "rgb_effect.hpp"


// ===== Sửa trong addList(), đoạn set màu border/background =====
//
// Code gốc (khoảng cuối hàm addList):
//
//   topBorder->setScaleX(0.945f);
//   topBorder->setScaleY(1.f);
//   topBorder->setPosition(ccp(161.25, 162.f));
//   ...
//   CCScale9Sprite *listBackground = ...
//   listBackground->setColor({0, 0, 0});
//   ...
//
// Sau khi set xong vị trí/scale như cũ, THÊM đoạn sau vào cuối addList()
// (sau dòng cuối cùng liên quan tới scrollbar, trước dấu } đóng hàm):

bool rgbEnabled = Mod::get()->getSettingValue<bool>("rgb_ui");

if (rgbEnabled) {
    // Xóa hiệu ứng RGB cũ nếu addList() được gọi lại (VD search/sort/reload)
    if (CCNode* oldFx = m_buttonMenu->getChildByID("rgb-effect"))
        oldFx->removeFromParentAndCleanup(true);

    std::vector<cocos2d::CCNode*> rgbTargets = {
        topBorder, bottomBorder, leftBorder, rightBorder, listBackground
    };

    RGBEffect* fx = RGBEffect::create(rgbTargets, /*speed*/ 60.f);
    fx->setID("rgb-effect");
    m_buttonMenu->addChild(fx);
} else {
    // Đảm bảo giữ đúng màu tĩnh khi setting đang tắt (không bị màu RGB
    // cũ còn sót lại nếu user vừa tắt setting rồi mở lại layer).
    cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");
    topBorder->setColor(color);
    bottomBorder->setColor(color);
    leftBorder->setColor(color);
    rightBorder->setColor(color);
}
