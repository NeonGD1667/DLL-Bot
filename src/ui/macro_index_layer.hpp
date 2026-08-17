#pragma once

#include "../includes.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

// Popup search + download macro từ 1 file JSON index host tùy ý
// (GitHub raw, Gist raw, self-host...).
//
// Setting "macro_index_url" trỏ tới file JSON có format:
//
// [
//   {
//     "level_name": "Bloodbath",
//     "level_id": 10565740,
//     "difficulty": "Extreme Demon",
//     "uploader": "NeonGD1667",
//     "rating": 4.8,
//     "format": "gdr2",
//     "download_url": "https://raw.githubusercontent.com/.../bloodbath.gdr2"
//   }
// ]
//
// Toàn bộ index được fetch 1 lần khi mở layer (hoặc bấm Refresh),
// sau đó search/filter chạy client-side trên dữ liệu đã tải.

class MacroIndexLayer : public geode::Popup, public TextInputDelegate {
private:
    struct MacroEntry {
        std::string levelName;
        int levelId = 0;
        std::string difficulty;
        std::string uploader;
        float rating = 0.f;
        std::string format;
        std::string downloadUrl;
    };

    TextInput* searchInput = nullptr;
    CCLabelBMFont* statusLabel = nullptr;
    CCMenuItemSpriteExtra* refreshBtn = nullptr;
    CCLayer* resultsLayer = nullptr;

    std::vector<MacroEntry> allEntries;
    std::vector<MacroEntry> filteredEntries;
    std::vector<CCNode*> resultNodes;

    geode::async::TaskHolder<geode::utils::web::WebResponse> indexTask;
    geode::async::TaskHolder<geode::utils::web::WebResponse> downloadTask;

    std::string currentDownloadName;
    std::string currentDownloadFormat;

    bool setup();

    void fetchIndex();
    void onIndexResult(geode::utils::web::WebResponse resp);

    void applyFilter(std::string const& query);
    void populateResults();
    void clearResults();

    void onDownloadClicked(CCObject* sender);
    void onDownloadResult(geode::utils::web::WebResponse resp);
    void onRefresh(CCObject*);

public:
    void textChanged(CCTextInputNode* node) override;

    static MacroIndexLayer* create();
    static void open();
};