#include "macro_index_layer.hpp"

using namespace geode::utils::web;

MacroIndexLayer* MacroIndexLayer::create() {
    MacroIndexLayer* ret = new MacroIndexLayer();
    if (ret->initAnchored(340.f, 260.f)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void MacroIndexLayer::open() {
    MacroIndexLayer* layer = create();
    layer->m_noElasticity = true;
    layer->show();
}

bool MacroIndexLayer::setup() {
    setTitle("Macro Index");

    searchInput = TextInput::create(240.f, "Search level name...", "chatFont.fnt");
    searchInput->setPosition({-20.f, 95.f});
    searchInput->setDelegate(this);
    m_mainLayer->addChild(searchInput);

    CCSprite* refreshSpr = CCSprite::createWithSpriteFrameName("GJ_updateBtn_001.png");
    refreshSpr->setScale(0.6f);
    refreshBtn = CCMenuItemSpriteExtra::create(
        refreshSpr, this, menu_selector(MacroIndexLayer::onRefresh));

    CCMenu* refreshMenu = CCMenu::create();
    refreshMenu->addChild(refreshBtn);
    refreshMenu->setPosition({130.f, 95.f});
    m_mainLayer->addChild(refreshMenu);

    statusLabel = CCLabelBMFont::create("Loading index...", "chatFont.fnt");
    statusLabel->setScale(0.5f);
    statusLabel->setOpacity(140);
    statusLabel->setPosition({0.f, 75.f});
    m_mainLayer->addChild(statusLabel);

    resultsLayer = CCLayer::create();
    resultsLayer->setPosition({0.f, 0.f});
    m_mainLayer->addChild(resultsLayer);

    fetchIndex();

    return true;
}

void MacroIndexLayer::onRefresh(CCObject*) {
    fetchIndex();
}

void MacroIndexLayer::fetchIndex() {
    statusLabel->setString("Loading index...");
    clearResults();
    allEntries.clear();

    std::string url = Mod::get()->getSettingValue<std::string>("macro_index_url");

    if (url.empty()) {
        statusLabel->setString("No index URL configured in settings.");
        return;
    }

    // spawn() tự hủy request cũ nếu đang chạy (VD bấm Refresh 2 lần liên
    // tiếp) và tự chạy callback trên main thread, an toàn để đụng Cocos node.
    indexTask.spawn(
        "Macro Index Fetch",
        WebRequest().get(url),
        [this](WebResponse resp) {
            onIndexResult(resp);
        }
    );
}

void MacroIndexLayer::onIndexResult(WebResponse resp) {
    if (!resp.ok()) {
        statusLabel->setString(
            fmt::format("Failed to load index (HTTP {}).", resp.code()).c_str());
        return;
    }

    std::string body = resp.string().unwrapOr("");
    auto json = nlohmann::json::parse(body, nullptr, false);

    if (json.is_discarded() || !json.is_array()) {
        statusLabel->setString("Index file is not valid JSON.");
        return;
    }

    allEntries.clear();
    for (auto const& item : json) {
        MacroEntry entry;
        entry.levelName = item.value("level_name", "Unknown");
        entry.levelId = item.value("level_id", 0);
        entry.difficulty = item.value("difficulty", "");
        entry.uploader = item.value("uploader", "N/A");
        entry.rating = item.value("rating", 0.f);
        entry.format = item.value("format", "gdr2");
        entry.downloadUrl = item.value("download_url", "");

        if (!entry.downloadUrl.empty())
            allEntries.push_back(entry);
    }

    statusLabel->setString(
        fmt::format("{} macro(s) loaded. Type to search.", allEntries.size()).c_str());

    // Nếu user đã gõ gì đó trước khi index load xong, áp filter luôn.
    std::string currentQuery = searchInput ? searchInput->getString() : "";
    if (!currentQuery.empty())
        applyFilter(currentQuery);
}

void MacroIndexLayer::textChanged(CCTextInputNode* node) {
    if (searchInput && node == searchInput->getInput()) {
        std::string query = searchInput->getString();

        if (query.empty()) {
            clearResults();
            statusLabel->setString(
                fmt::format("{} macro(s) loaded. Type to search.", allEntries.size()).c_str());
            return;
        }

        applyFilter(query);
    }
}

void MacroIndexLayer::applyFilter(std::string const& query) {
    std::string lowerQuery = Utils::toLower(query);

    filteredEntries.clear();
    for (MacroEntry const& entry : allEntries) {
        if (Utils::toLower(entry.levelName).find(lowerQuery) != std::string::npos)
            filteredEntries.push_back(entry);
    }

    if (filteredEntries.empty())
        statusLabel->setString("No macros found for this level.");
    else
        statusLabel->setString(
            fmt::format("{} result(s).", filteredEntries.size()).c_str());

    populateResults();
}

void MacroIndexLayer::clearResults() {
    for (CCNode* node : resultNodes)
        node->removeFromParentAndCleanup(true);
    resultNodes.clear();
    filteredEntries.clear();
}

void MacroIndexLayer::populateResults() {
    for (CCNode* node : resultNodes)
        node->removeFromParentAndCleanup(true);
    resultNodes.clear();

    constexpr size_t maxShown = 5;
    float yStart = 55.f;
    float rowHeight = 32.f;

    for (size_t i = 0; i < filteredEntries.size() && i < maxShown; i++) {
        MacroEntry const& r = filteredEntries[i];
        float y = yStart - (i * rowHeight);

        CCLabelBMFont* nameLbl = CCLabelBMFont::create(
            r.levelName.c_str(), "bigFont.fnt");
        nameLbl->setAnchorPoint({0.f, 0.5f});
        nameLbl->setScale(0.42f);
        nameLbl->setPosition({-155.f, y + 6.f});
        resultsLayer->addChild(nameLbl);
        resultNodes.push_back(nameLbl);

        std::string metaStr = fmt::format(
            "{} | by {} | {:.1f}\u2605", r.difficulty, r.uploader, r.rating);
        CCLabelBMFont* metaLbl = CCLabelBMFont::create(
            metaStr.c_str(), "chatFont.fnt");
        metaLbl->setAnchorPoint({0.f, 0.5f});
        metaLbl->setScale(0.35f);
        metaLbl->setOpacity(140);
        metaLbl->setPosition({-155.f, y - 6.f});
        resultsLayer->addChild(metaLbl);
        resultNodes.push_back(metaLbl);

        ButtonSprite* dlSpr = ButtonSprite::create("Download");
        dlSpr->setScale(0.4f);
        CCMenuItemSpriteExtra* dlBtn = CCMenuItemSpriteExtra::create(
            dlSpr, this, menu_selector(MacroIndexLayer::onDownloadClicked));
        dlBtn->setPosition({140.f, y});
        dlBtn->setTag(static_cast<int>(i));

        CCMenu* rowMenu = CCMenu::create();
        rowMenu->setPosition({0.f, 0.f});
        rowMenu->addChild(dlBtn);
        resultsLayer->addChild(rowMenu);
        resultNodes.push_back(rowMenu);
    }
}

void MacroIndexLayer::onDownloadClicked(CCObject* sender) {
    int idx = static_cast<CCNode*>(sender)->getTag();
    if (idx < 0 || static_cast<size_t>(idx) >= filteredEntries.size())
        return;

    MacroEntry const& r = filteredEntries[idx];

    currentDownloadName = r.levelName;
    currentDownloadFormat = r.format;

    statusLabel->setString(
        fmt::format("Downloading \"{}\"...", r.levelName).c_str());

    downloadTask.spawn(
        "Macro Download",
        WebRequest().get(r.downloadUrl),
        [this](WebResponse resp) {
            onDownloadResult(resp);
        }
    );
}

void MacroIndexLayer::onDownloadResult(WebResponse resp) {
    if (!resp.ok()) {
        statusLabel->setString(
            fmt::format("Download failed (HTTP {}).", resp.code()).c_str());
        return;
    }

    std::vector<uint8_t> data = resp.data();
    if (data.empty()) {
        statusLabel->setString("Downloaded file is empty.");
        return;
    }

    std::filesystem::path macrosFolder =
        Mod::get()->getSettingValue<std::filesystem::path>("macros_folder");

    if (!std::filesystem::exists(macrosFolder))
        utils::file::createDirectoryAll(macrosFolder);

    std::string extension = "." + currentDownloadFormat;
    std::filesystem::path finalPath = macrosFolder / (currentDownloadName + extension);

    int iterations = 0;
    while (std::filesystem::exists(finalPath)) {
        iterations++;
        finalPath = macrosFolder /
            fmt::format("{} ({}){}", currentDownloadName, iterations, extension);
    }

    std::ofstream f(finalPath, std::ios::binary);
    if (!f) {
        statusLabel->setString("Failed to write macro file.");
        return;
    }

    f.write(reinterpret_cast<const char*>(data.data()), data.size());
    f.close();

    statusLabel->setString(
        fmt::format("Downloaded \"{}\".", currentDownloadName).c_str());
    Notification::create("Macro Downloaded", NotificationIcon::Success)->show();
}
