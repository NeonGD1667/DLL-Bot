#include "macro.hpp"

#include "ui/game_ui.hpp"
#include "ui/record_layer.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <Geode/modify/PlayLayer.hpp>

namespace {

static std::string xdJoin(
    int frame,
    bool down,
    int button,
    bool player2,
    bool posOnly
) {
    std::ostringstream ss;

    ss << frame << '|'
       << (down ? 1 : 0) << '|'
       << button << '|'
       << (player2 ? 1 : 0) << '|'
       << (posOnly ? 1 : 0);

    return ss.str();
}

static std::string xdFrameFix(
    int frame,
    gdr::FrameFix const& fix
) {
    /*
     * XD frame-fix layout expected by the existing importer:

       frame
       hold
       button
       player2
       posOnly
       p1.x
       p1.y
       ...
       p2.x
       p2.y

     * We don't have every XD-only physics value in Macro,
     * therefore preserve the known position fields and zero
     * the unknown fields.
     */

    std::ostringstream ss;
    ss << std::setprecision(9);

    ss << frame
       << "|0"
       << "|0"
       << "|0"
       << "|1"

       << "|" << fix.p1.position.x
       << "|" << fix.p1.position.y

       << "|0"
       << "|0"
       << "|0"
       << "|0"

       << "|" << fix.p2.position.x
       << "|" << fix.p2.position.y;

    return ss.str();
}

static bool xdParseBool(std::string const& value) {
    return value == "1" ||
           value == "true" ||
           value == "TRUE";
}

static bool xdParseInt(
    std::string const& value,
    int& out
) {
    try {
        size_t pos = 0;
        int result = std::stoi(value, &pos);

        if (pos != value.size())
            return false;

        out = result;
        return true;
    }
    catch (...) {
        return false;
    }
}

static bool xdParseFloat(
    std::string const& value,
    float& out
) {
    try {
        size_t pos = 0;
        float result = std::stof(value, &pos);

        if (pos != value.size())
            return false;

        out = result;
        return true;
    }
    catch (...) {
        return false;
    }
}

} // namespace

int Macro::saveXD(
    std::string author,
    std::string desc,
    std::string path
) {
    if (inputs.empty())
        return 31;

    if (path.empty())
        return 20;

    if (path.ends_with(".xd"))
        path.erase(path.size() - 3);

    int iteration = 0;
    std::string finalPath = path + ".xd";

    while (std::filesystem::exists(finalPath)) {
        ++iteration;
        finalPath =
            path + fmt::format(" ({}).xd", iteration);
    }

    std::ofstream file;

#ifdef GEODE_IS_WINDOWS
    std::wstring widePath = Utils::widen(finalPath);

    if (widePath == L"Widen Error")
        return 30;

    file.open(widePath, std::ios::binary);
#else
    file.open(finalPath, std::ios::binary);
#endif

    if (!file)
        return 20;

    /*
     * XD uses an FPS header.

       240 -> multiplier 1
       120 -> multiplier 2
       60  -> multiplier 4

     * Existing importer does:
     *
     *     frame = xdFrame * (240 / fps)
     */

    float fps = framerate;

    if (!std::isfinite(fps) || fps <= 0.f)
        fps = 240.f;

    /*
     * XD has a special Android marker in the importer.
     * For normal export we write the actual FPS instead.
     */
    int xdFPS = static_cast<int>(
        std::round(fps)
    );

    if (xdFPS <= 0)
        xdFPS = 240;

    file << xdFPS << '\n';

    /*
     * Sort a COPY only.
     *
     * Do NOT reorder Macro::inputs itself.
     */
    std::vector<input const*> ordered;
    ordered.reserve(inputs.size());

    for (auto const& in : inputs)
        ordered.push_back(&in);

    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [](input const* a, input const* b) {
            if (a->frame != b->frame)
                return a->frame < b->frame;

            /*
             * Keep player 1 before player 2.
             */
            if (a->player2 != b->player2)
                return !a->player2;

            return false;
        }
    );

    const double multiplier =
        240.0 / static_cast<double>(xdFPS);

    /*
     * Convert internal 240-TPS frame back to XD frame.
     *
     * Example:
     *
     * internal 240
     * FPS 120
     * XD frame = 120
     */
    for (auto const* in : ordered) {
        int xdFrame = static_cast<int>(
            std::llround(
                static_cast<double>(in->frame) /
                multiplier
            )
        );

        if (xdFrame < 0)
            xdFrame = 0;

        file << xdJoin(
            xdFrame,
            in->down,
            in->button,
            in->player2,
            false
        ) << '\n';
    }

    /*
     * Frame fixes are optional.
     *
     * They use the same posOnly structure accepted
     * by the current XDtoGDR importer.
     */
    for (auto const& fix : frameFixes) {
        int xdFrame = static_cast<int>(
            std::llround(
                static_cast<double>(fix.frame) /
                multiplier
            )
        );

        if (xdFrame < 0)
            xdFrame = 0;

        file << xdFrame << "|0|0|0|1";

        file << '|'
             << std::setprecision(9)
             << fix.p1.position.x;

        file << '|'
             << std::setprecision(9)
             << fix.p1.position.y;

        file << "|0|0|0|0";

        file << '|'
             << std::setprecision(9)
             << fix.p2.position.x;

        file << '|'
             << std::setprecision(9)
             << fix.p2.position.y;

        file << '\n';
    }

    if (!file.good()) {
        file.close();
        return 21;
    }

    file.close();

    log::info(
        "Saved XD macro: {} ({} inputs)",
        finalPath,
        inputs.size()
    );

    return 0;
}

bool Macro::loadXDFile(
    std::filesystem::path path
) {
    Macro newMacro = Macro::XDtoGDR(path);

    if (newMacro.description == "fail")
        return false;

    Global::get().macro = std::move(newMacro);

    return true;
}

Macro Macro::XDtoGDR(
    std::filesystem::path path
) {
    Macro newMacro;

    newMacro.author = "N/A";
    newMacro.description = "N/A";
    newMacro.gameVersion = GEODE_GD_VERSION;
    newMacro.framerate = 240.f;
    newMacro.xdBotMacro = true;
    newMacro.botInfo.name = "xdBot";
    newMacro.botInfo.version = xdBotVersion;

    std::ifstream file;

#ifdef GEODE_IS_WINDOWS
    file.open(
        Utils::widen(path.string()),
        std::ios::binary
    );
#else
    file.open(
        path,
        std::ios::binary
    );
#endif

    if (!file.is_open()) {
        newMacro.description = "fail";
        return newMacro;
    }

    std::string line;

    /*
     * XD Android files use:

         android

     * Normal XD files use:

         FPS

     * Existing importer treated Android as 4x,
     * meaning Android recordings are interpreted
     * as 60 FPS relative to 240 TPS.
     */
    float fpsMultiplier = 1.f;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty())
            continue;

        std::vector<std::string> action;
        std::stringstream ss(line);
        std::string item;

        while (std::getline(ss, item, '|'))
            action.push_back(item);

        /*
         * Header.
         */
        if (firstLine) {
            firstLine = false;

            if (action.size() == 1) {
                if (action[0] == "android") {
                    fpsMultiplier = 4.f;
                    newMacro.framerate = 60.f;
                    continue;
                }

                int fps = 0;

                if (xdParseInt(action[0], fps) && fps > 0) {
                    fpsMultiplier =
                        240.f / static_cast<float>(fps);

                    newMacro.framerate =
                        static_cast<float>(fps);

                    continue;
                }
            }
        }

        /*
         * Some XD files may contain a header anywhere
         * before the first input.
         */
        if (action.size() == 1) {
            if (action[0] == "android") {
                fpsMultiplier = 4.f;
                newMacro.framerate = 60.f;
                continue;
            }

            int fps = 0;

            if (xdParseInt(action[0], fps) && fps > 0) {
                fpsMultiplier =
                    240.f / static_cast<float>(fps);

                newMacro.framerate =
                    static_cast<float>(fps);

                continue;
            }
        }

        /*
         * Normal XD action needs at least:

         0 frame
         1 hold
         2 button
         3 player2
         4 posOnly
         */
        if (action.size() < 5) {
            log::warn(
                "Skipping malformed XD line: {}",
                line
            );
            continue;
        }

        int rawFrame = 0;
        int button = 0;

        if (!xdParseInt(action[0], rawFrame) ||
            !xdParseInt(action[2], button)) {
            log::warn(
                "Skipping invalid XD input: {}",
                line
            );
            continue;
        }

        bool hold = xdParseBool(action[1]);
        bool player2 = xdParseBool(action[3]);
        bool posOnly = xdParseBool(action[4]);

        int frame = static_cast<int>(
            std::llround(
                static_cast<double>(rawFrame) *
                static_cast<double>(fpsMultiplier)
            )
        );

        if (frame < 0)
            frame = 0;

        if (!posOnly) {
            newMacro.inputs.emplace_back(
                frame,
                button,
                player2,
                hold
            );

            continue;
        }

        /*
         * Frame-fix requires:
         *
         * [5]  p1.x
         * [6]  p1.y
         *
         * [11] p2.x
         * [12] p2.y
         */
        if (action.size() < 13) {
            log::warn(
                "Skipping malformed XD frame-fix: {}",
                line
            );
            continue;
        }

        float p1x = 0.f;
        float p1y = 0.f;
        float p2x = 0.f;
        float p2y = 0.f;

        if (!xdParseFloat(action[5], p1x) ||
            !xdParseFloat(action[6], p1y) ||
            !xdParseFloat(action[11], p2x) ||
            !xdParseFloat(action[12], p2y)) {
            log::warn(
                "Skipping invalid XD frame-fix: {}",
                line
            );
            continue;
        }

        cocos2d::CCPoint p1Pos =
            ccp(p1x, p1y);

        cocos2d::CCPoint p2Pos =
            ccp(p2x, p2y);

        newMacro.frameFixes.push_back({
            frame,
            {p1Pos, 0.f, false},
            {p2Pos, 0.f, false}
        });
    }

    file.close();

    /*
     * Last frame.
     */
    if (!newMacro.inputs.empty()) {
        newMacro.lastRecordedFrame =
            newMacro.inputs.back().frame;

        newMacro.duration =
            static_cast<double>(
                newMacro.lastRecordedFrame
            ) / static_cast<double>(
                newMacro.framerate > 0.f
                    ? newMacro.framerate
                    : 240.f
            );
    }

    return newMacro;
}
