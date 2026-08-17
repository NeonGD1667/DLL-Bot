#include "macro.hpp"

#include "includes.hpp"
#include "utils/utils.hpp"

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
    auto& macro = Global::get().macro;

    if (macro.inputs.empty())
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

    file.open(
        widePath,
        std::ios::binary
    );
#else
    file.open(
        finalPath,
        std::ios::binary
    );
#endif

    if (!file)
        return 20;

    float fps = macro.framerate;

    if (!std::isfinite(fps) || fps <= 0.f)
        fps = 240.f;

    int xdFPS = static_cast<int>(
        std::round(fps)
    );

    if (xdFPS <= 0)
        xdFPS = 240;

    file << xdFPS << '\n';

    std::vector<input const*> ordered;

    ordered.reserve(
        macro.inputs.size()
    );

    for (auto const& in : macro.inputs)
        ordered.push_back(&in);

    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [](input const* a, input const* b) {
            if (a->frame != b->frame)
                return a->frame < b->frame;

            if (a->player2 != b->player2)
                return !a->player2;

            return false;
        }
    );

    const double multiplier =
        240.0 /
        static_cast<double>(xdFPS);

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
     * Frame fixes are intentionally not exported here.
     *
     * The current gdr::FrameData API does not expose
     * the old .position member used by the previous
     * implementation.
     *
     * Keeping the input export independent prevents
     * the XD exporter from depending on an outdated
     * FrameData layout.
     */

    if (!file.good()) {
        file.close();
        return 21;
    }

    file.close();

    log::info(
        "Saved XD macro: {} ({} inputs)",
        finalPath,
        macro.inputs.size()
    );

    return 0;
}


bool Macro::loadXDFile(
    std::filesystem::path path
) {
    Macro newMacro =
        Macro::XDtoGDR(path);

    if (newMacro.description == "fail")
        return false;

    Global::get().macro =
        std::move(newMacro);

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

    float fpsMultiplier = 1.f;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (!line.empty() &&
            line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty())
            continue;

        std::vector<std::string> action;

        std::stringstream ss(line);
        std::string item;

        while (std::getline(ss, item, '|'))
            action.push_back(item);

        /*
         * Header
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

                if (
                    xdParseInt(action[0], fps) &&
                    fps > 0
                ) {
                    fpsMultiplier =
                        240.f /
                        static_cast<float>(fps);

                    newMacro.framerate =
                        static_cast<float>(fps);

                    continue;
                }
            }
        }

        /*
         * Header can also appear later.
         */
        if (action.size() == 1) {
            if (action[0] == "android") {
                fpsMultiplier = 4.f;
                newMacro.framerate = 60.f;
                continue;
            }

            int fps = 0;

            if (
                xdParseInt(action[0], fps) &&
                fps > 0
            ) {
                fpsMultiplier =
                    240.f /
                    static_cast<float>(fps);

                newMacro.framerate =
                    static_cast<float>(fps);

                continue;
            }
        }

        /*
         * Normal XD action:
         *
         * 0 = frame
         * 1 = hold
         * 2 = button
         * 3 = player2
         * 4 = posOnly
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

        if (
            !xdParseInt(action[0], rawFrame) ||
            !xdParseInt(action[2], button)
        ) {
            log::warn(
                "Skipping invalid XD input: {}",
                line
            );

            continue;
        }

        bool hold =
            xdParseBool(action[1]);

        bool player2 =
            xdParseBool(action[3]);

        bool posOnly =
            xdParseBool(action[4]);

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
         * Frame-fix records are accepted only
         * when they contain the expected number
         * of fields.
         *
         * The current exporter does not generate
         * them, but keeping this parser allows
         * existing XD files to remain readable.
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

        if (
            !xdParseFloat(action[5], p1x) ||
            !xdParseFloat(action[6], p1y) ||
            !xdParseFloat(action[11], p2x) ||
            !xdParseFloat(action[12], p2y)
        ) {
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

    if (!newMacro.inputs.empty()) {
        newMacro.lastRecordedFrame =
            newMacro.inputs.back().frame;

        newMacro.duration =
            static_cast<double>(
                newMacro.lastRecordedFrame
            ) /
            static_cast<double>(
                newMacro.framerate > 0.f
                    ? newMacro.framerate
                    : 240.f
            );
    }

    return newMacro;
}