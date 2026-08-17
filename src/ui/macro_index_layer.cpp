#include "macro_index_layer.hpp"

using namespace geode::utils::web;

MacroIndexLayer* MacroIndexLayer::create() {
    auto ret = new MacroIndexLayer();

    if (ret->init(380.f, 320.f)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

void MacroIndexLayer::open() {
    auto layer = create();

    if (!layer)
        return;

    layer->m_noElasticity = true;
    layer->show();
}

bool MacroIndexLayer::setup() {
    setTitle("Macro Index");

    // Search
    searchInput = TextInput::create(
        275.f,
        "Search level / macro...",
        "chatFont.fnt"
    );
    searchInput->setPosition({-25.f, 130.f});
    searchInput->setDelegate(this);
    m_mainLayer->addChild(searchInput);

    // Refresh
    CCSprite* refreshSpr =
        CCSprite::createWithSpriteFrameName("GJ_updateBtn_001.png");

    refreshSpr->setScale(0.6f);

    refreshBtn = CCMenuItemSpriteExtra::create(
        refreshSpr,
        this,
        menu_selector(MacroIndexLayer::onRefresh)
    );

    CCMenu* refreshMenu = CCMenu::create();
    refreshMenu->addChild(refreshBtn);
    refreshMenu->setPosition({150.f, 130.f});
    m_mainLayer->addChild(refreshMenu);

    // Status
    statusLabel = CCLabelBMFont::create(
        "Loading index...",
        "chatFont.fnt"
    );
    statusLabel->setScale(0.5f);
    statusLabel->setOpacity(140);
    statusLabel->setPosition({0.f, 106.f});
    m_mainLayer->addChild(statusLabel);

    // Results
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

    std::string url =
        Mod::get()->getSettingValue<std::string>("macro_index_url");

    if (url.empty()) {
        statusLabel->setString(
            "No index URL configured in settings."
        );
        return;
    }

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
            fmt::format(
                "Failed to load index (HTTP {}).",
                resp.code()
            ).c_str()
        );
        return;
    }

    std::string body = resp.string().unwrapOr("");

    auto json = nlohmann::json::parse(
        body,
        nullptr,
        false
    );

    if (json.is_discarded() || !json.is_array()) {
        statusLabel->setString(
            "Index file is not valid JSON."
        );
        return;
    }

    allEntries.clear();

    for (auto const& item : json) {
        MacroEntry entry;

        entry.levelName =
            item.value("level_name", "Unknown");

        entry.levelId =
            item.value("level_id", 0);

        entry.difficulty =
            item.value("difficulty", "");

        entry.uploader =
            item.value("uploader", "N/A");

        entry.rating =
            item.value("rating", 0.f);

        entry.format =
            item.value("format", "gdr2");

        entry.downloadUrl =
            item.value("download_url", "");

        if (!entry.downloadUrl.empty())
            allEntries.push_back(entry);
    }

    statusLabel->setString(
        fmt::format(
            "{} macro(s) loaded. Type to search.",
            allEntries.size()
        ).c_str()
    );

    std::string currentQuery =
        searchInput
            ? searchInput->getString()
            : "";

    if (!currentQuery.empty())
        applyFilter(currentQuery);
}

void MacroIndexLayer::textChanged(CCTextInputNode* node) {
    if (!node)
        return;

    std::string query = node->getString();

    if (query.empty()) {
        clearResults();

        statusLabel->setString(
            fmt::format(
                "{} macro(s) loaded. Type to search.",
                allEntries.size()
            ).c_str()
        );

        return;
    }

    applyFilter(query);
}

void MacroIndexLayer::applyFilter(
    std::string const& query
) {
    std::string lowerQuery =
        Utils::toLower(query);

    filteredEntries.clear();

    for (MacroEntry const& entry : allEntries) {
        if (
            Utils::toLower(entry.levelName)
                .find(lowerQuery)
            != std::string::npos
        ) {
            filteredEntries.push_back(entry);
        }
    }

    if (filteredEntries.empty()) {
        statusLabel->setString(
            "No macros found for this level."
        );
    }
    else {
        statusLabel->setString(
            fmt::format(
                "{} result(s).",
                filteredEntries.size()
            ).c_str()
        );
    }

    populateResults();
}

void MacroIndexLayer::clearResults() {
    for (CCNode* node : resultNodes) {
        node->removeFromParentAndCleanup(true);
    }

    resultNodes.clear();
    filteredEntries.clear();
}

void MacroIndexLayer::populateResults() {
    for (CCNode* node : resultNodes) {
        node->removeFromParentAndCleanup(true);
    }

    resultNodes.clear();

    constexpr size_t maxShown = 5;

    // Giữ khoảng cách vừa đủ cho popup 380x320.
    float yStart = 78.f;
    float rowHeight = 45.f;

    for (
        size_t i = 0;
        i < filteredEntries.size() && i < maxShown;
        i++
    ) {
        MacroEntry const& r = filteredEntries[i];

        float y =
            yStart - (i * rowHeight);

        // Macro name
        CCLabelBMFont* nameLbl =
            CCLabelBMFont::create(
                r.levelName.c_str(),
                "bigFont.fnt"
            );

        nameLbl->setAnchorPoint({0.f, 0.5f});
        nameLbl->setScale(0.42f);
        nameLbl->setPosition(
            {-160.f, y + 8.f}
        );

        resultsLayer->addChild(nameLbl);
        resultNodes.push_back(nameLbl);

        // Author + level
        std::string metaStr =
            fmt::format(
                "by {} \u2022 Level {}",
                r.uploader,
                r.levelId
            );

        CCLabelBMFont* metaLbl =
            CCLabelBMFont::create(
                metaStr.c_str(),
                "chatFont.fnt"
            );

        metaLbl->setAnchorPoint({0.f, 0.5f});
        metaLbl->setScale(0.35f);
        metaLbl->setOpacity(160);
        metaLbl->setPosition(
            {-160.f, y - 9.f}
        );

        resultsLayer->addChild(metaLbl);
        resultNodes.push_back(metaLbl);

        // Download button
        ButtonSprite* dlSpr =
            ButtonSprite::create("Download");

        dlSpr->setScale(0.42f);

        CCMenuItemSpriteExtra* dlBtn =
            CCMenuItemSpriteExtra::create(
                dlSpr,
                this,
                menu_selector(
                    MacroIndexLayer::onDownloadClicked
                )
            );

        dlBtn->setPosition({140.f, y});
        dlBtn->setTag(static_cast<int>(i));

        CCMenu* rowMenu =
            CCMenu::create();

        rowMenu->setPosition({0.f, 0.f});
        rowMenu->addChild(dlBtn);

        resultsLayer->addChild(rowMenu);
        resultNodes.push_back(rowMenu);
    }
}

void MacroIndexLayer::onDownloadClicked(
    CCObject* sender
) {
    int idx =
        static_cast<CCNode*>(sender)->getTag();

    if (
        idx < 0 ||
        static_cast<size_t>(idx)
            >= filteredEntries.size()
    ) {
        return;
    }

    MacroEntry const& r =
        filteredEntries[idx];

    currentDownloadName =
        r.levelName;

    currentDownloadFormat =
        r.format;

    statusLabel->setString(
        fmt::format(
            "Downloading \"{}\"...",
            r.levelName
        ).c_str()
    );

    downloadTask.spawn(
        "Macro Download",
        WebRequest().get(r.downloadUrl),
        [this](WebResponse resp) {
            onDownloadResult(resp);
        }
    );
}

void MacroIndexLayer::onDownloadResult(
    WebResponse resp
) {
    if (!resp.ok()) {
        statusLabel->setString(
            fmt::format(
                "Download failed (HTTP {}).",
                resp.code()
            ).c_str()
        );
        return;
    }

    std::vector<uint8_t> data =
        resp.data();

    if (data.empty()) {
        statusLabel->setString(
            "Downloaded file is empty."
        );
        return;
    }

    std::filesystem::path macrosFolder =
        Mod::get()->getSettingValue<
            std::filesystem::path
        >("macros_folder");

    if (!std::filesystem::exists(macrosFolder)) {
        auto result =
            utils::file::createDirectoryAll(
                macrosFolder
            );

        if (!result) {
            statusLabel->setString(
                "Failed to create macros folder."
            );
            return;
        }
    }

    std::string extension =
        "." + currentDownloadFormat;

    std::filesystem::path finalPath =
        macrosFolder /
        (currentDownloadName + extension);

    int iterations = 0;

    while (std::filesystem::exists(finalPath)) {
        iterations++;

        finalPath =
            macrosFolder /
            fmt::format(
                "{} ({}){}",
                currentDownloadName,
                iterations,
                extension
            );
    }

    std::ofstream f(
        finalPath,
        std::ios::binary
    );

    if (!f) {
        statusLabel->setString(
            "Failed to write macro file."
        );
        return;
    }

    f.write(
        reinterpret_cast<const char*>(
            data.data()
        ),
        data.size()
    );

    f.close();

    statusLabel->setString(
        fmt::format(
            "Downloaded \"{}\".",
            currentDownloadName
        ).c_str()
    );

    Notification::create(
        "Macro Downloaded",
        NotificationIcon::Success
    )->show();
}
