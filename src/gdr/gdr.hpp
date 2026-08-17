#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
#include <optional>
#include <sstream>

#include "json.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/VersionInfo.hpp>

geode::prelude::VersionInfo getVersion(std::vector<std::string> nums);

cocos2d::CCPoint dataFromString(std::string const& dataString);

std::vector<std::string> splitByChar(
    std::string const& str,
    char splitChar
);

const std::string xdBotVersion = "v2.6-Nako";

namespace gdr {

using namespace nlohmann;

struct Bot {
    std::string name;
    std::string version;

    Bot() = default;

    Bot(
        std::string const& name,
        std::string const& version
    )
        : name(name),
          version(version) {}
};

struct Level {
    uint32_t id = 0;
    std::string name;

    Level() = default;

    Level(
        std::string const& name,
        uint32_t id = 0
    )
        : id(id),
          name(name) {}
};

struct FrameData {
    cocos2d::CCPoint pos = {0.f, 0.f};
    float rotation = 0.f;
    bool rotate = true;
};

struct FrameFix {
    int frame = 0;
    FrameData p1;
    FrameData p2;
};

class Input {
protected:
    Input() = default;

    template <typename, typename>
    friend class Replay;

public:
    uint32_t frame = 0;
    int button = 0;
    bool player2 = false;
    bool down = false;

    virtual ~Input() = default;

    virtual void parseExtension(
        json::object_t obj
    ) {}

    virtual json::object_t saveExtension() const {
        return {};
    }

    Input(
        int frame,
        int button,
        bool player2,
        bool down
    )
        : frame(static_cast<uint32_t>(frame)),
          button(button),
          player2(player2),
          down(down) {}

    static Input hold(
        int frame,
        int button,
        bool player2 = false
    ) {
        return Input(
            frame,
            button,
            player2,
            true
        );
    }

    static Input release(
        int frame,
        int button,
        bool player2 = false
    ) {
        return Input(
            frame,
            button,
            player2,
            false
        );
    }

    bool operator<(Input const& other) const {
        return frame < other.frame;
    }
};

template <
    typename S = void,
    typename T = Input
>
class Replay {
    Replay() = default;

public:
    using InputType = T;

    using Self = std::conditional_t<
        std::is_same_v<S, void>,
        Replay<S, T>,
        S
    >;

    std::string author;
    std::string description;

    float duration = 0.f;
    float gameVersion = 0.f;
    float version = 1.0f;

    float framerate = 240.f;

    int seed = 0;
    int coins = 0;

    bool ldm = false;

    Bot botInfo;
    Level levelInfo;

    std::vector<InputType> inputs;
    std::vector<FrameFix> frameFixes;

    int lastRecordedFrame = 0;

    uint32_t frameForTime(double time) const {
        if (time <= 0.0)
            return 0;

        return static_cast<uint32_t>(
            time * static_cast<double>(framerate)
        );
    }

    virtual void parseExtension(
        json::object_t obj
    ) {}

    virtual json::object_t saveExtension() const {
        return {};
    }

    Replay(
        std::string const& botName,
        std::string const& botVersion
    )
        : botInfo(
            botName,
            botVersion
        ) {}

    static Self importData(
        std::vector<uint8_t> const& data,
        bool importInputs = true
    ) {
        Self replay;
        json replayJson;

        replayJson =
            json::from_msgpack(
                data,
                true,
                false
            );

        if (replayJson.is_discarded()) {
            replayJson =
                json::parse(
                    data,
                    nullptr,
                    false
                );

            if (replayJson.is_discarded())
                return replay;
        }

        if (
            replayJson.contains("gameVersion") &&
            !replayJson["gameVersion"].is_null()
        )
            replay.gameVersion =
                replayJson["gameVersion"].get<float>();

        if (
            replayJson.contains("description") &&
            !replayJson["description"].is_null()
        )
            replay.description =
                replayJson["description"].get<std::string>();

        if (
            replayJson.contains("version") &&
            !replayJson["version"].is_null()
        )
            replay.version =
                replayJson["version"].get<float>();

        if (
            replayJson.contains("duration") &&
            !replayJson["duration"].is_null()
        )
            replay.duration =
                replayJson["duration"].get<float>();

        if (
            replayJson.contains("author") &&
            !replayJson["author"].is_null()
        )
            replay.author =
                replayJson["author"].get<std::string>();

        if (
            replayJson.contains("seed") &&
            !replayJson["seed"].is_null()
        )
            replay.seed =
                replayJson["seed"].get<int>();

        if (
            replayJson.contains("coins") &&
            !replayJson["coins"].is_null()
        )
            replay.coins =
                replayJson["coins"].get<int>();

        if (
            replayJson.contains("ldm") &&
            !replayJson["ldm"].is_null()
        )
            replay.ldm =
                replayJson["ldm"].get<bool>();

        if (
            replayJson.contains("lastRecordedFrame") &&
            !replayJson["lastRecordedFrame"].is_null()
        )
            replay.lastRecordedFrame =
                replayJson["lastRecordedFrame"].get<int>();

        if (
            replayJson.contains("framerate") &&
            !replayJson["framerate"].is_null()
        )
            replay.framerate =
                replayJson["framerate"].get<float>();

        if (
            replayJson.contains("bot") &&
            replayJson["bot"].is_object()
        ) {
            if (
                replayJson["bot"].contains("name") &&
                !replayJson["bot"]["name"].is_null()
            )
                replay.botInfo.name =
                    replayJson["bot"]["name"].get<std::string>();

            if (
                replayJson["bot"].contains("version") &&
                !replayJson["bot"]["version"].is_null()
            )
                replay.botInfo.version =
                    replayJson["bot"]["version"].get<std::string>();
        }

        if (
            replayJson.contains("level") &&
            replayJson["level"].is_object()
        ) {
            if (
                replayJson["level"].contains("id") &&
                !replayJson["level"]["id"].is_null()
            )
                replay.levelInfo.id =
                    replayJson["level"]["id"].get<uint32_t>();

            if (
                replayJson["level"].contains("name") &&
                !replayJson["level"]["name"].is_null()
            )
                replay.levelInfo.name =
                    replayJson["level"]["name"].get<std::string>();
        }

        std::string ver =
            replay.botInfo.version;

        bool rotation =
            ver.find("beta.") == std::string::npos &&
            ver.find("alpha.") == std::string::npos;

        if (
            replay.botInfo.name == "xdBot" &&
            ver == "v2.0.0"
        )
            rotation = true;

        int offset =
            replay.botInfo.name == "xdBot"
                ? 1
                : 0;

        if (offset == 1) {
            if (
                !ver.empty() &&
                ver.front() == 'v'
            )
                ver = ver.substr(1);

            auto splitVer =
                splitByChar(ver, '.');

            if (splitVer.size() <= 3) {
                std::vector<std::string> realVer = {
                    "2",
                    "3",
                    "6"
                };

                auto macroVer =
                    getVersion(splitVer);

                auto checkVer =
                    getVersion(realVer);

                if (macroVer >= checkVer)
                    offset = 0;
            }
        }

        replay.parseExtension(
            replayJson.get<json::object_t>()
        );

        if (!importInputs)
            return replay;

        if (
            replayJson.contains("inputs") &&
            replayJson["inputs"].is_array()
        ) {
            for (
                json const& inputJson :
                replayJson["inputs"]
            ) {
                if (!inputJson.is_object())
                    continue;

                if (
                    !inputJson.contains("frame") ||
                    inputJson["frame"].is_null()
                )
                    continue;

                if (
                    !inputJson.contains("btn") ||
                    !inputJson.contains("2p") ||
                    !inputJson.contains("down")
                )
                    continue;

                InputType input;

                input.frame =
                    static_cast<uint32_t>(
                        inputJson["frame"].get<int>() +
                        offset
                    );

                input.button =
                    inputJson["btn"].get<int>();

                input.player2 =
                    inputJson["2p"].get<bool>();

                input.down =
                    inputJson["down"].get<bool>();

                input.parseExtension(
                    inputJson.get<json::object_t>()
                );

                replay.inputs.push_back(
                    std::move(input)
                );
            }
        }

        if (
            !replayJson.contains("frameFixes") ||
            !replayJson["frameFixes"].is_array()
        )
            return replay;

        for (
            json const& frameFixJson :
            replayJson["frameFixes"]
        ) {
            if (!frameFixJson.is_object())
                continue;

            if (
                !frameFixJson.contains("frame") ||
                frameFixJson["frame"].is_null()
            )
                continue;

            FrameFix frameFix;

            frameFix.frame =
                frameFixJson["frame"].get<int>() +
                offset;

            if (
                frameFixJson.contains("player1") &&
                frameFixJson.contains("player2")
            ) {
                frameFix.p1.pos =
                    dataFromString(
                        frameFixJson["player1"]
                            .get<std::string>()
                    );

                frameFix.p1.rotate = false;

                frameFix.p2.pos =
                    dataFromString(
                        frameFixJson["player2"]
                            .get<std::string>()
                    );

                frameFix.p2.rotate = false;
            }

            else if (
                frameFixJson.contains("player1X") &&
                frameFixJson.contains("player1Y") &&
                frameFixJson.contains("player2X") &&
                frameFixJson.contains("player2Y")
            ) {
                frameFix.p1.pos =
                    ccp(
                        frameFixJson["player1X"].get<float>(),
                        frameFixJson["player1Y"].get<float>()
                    );

                frameFix.p1.rotate = false;

                frameFix.p2.pos =
                    ccp(
                        frameFixJson["player2X"].get<float>(),
                        frameFixJson["player2Y"].get<float>()
                    );

                frameFix.p2.rotate = false;
            }

            else if (
                frameFixJson.contains("p1") &&
                frameFixJson["p1"].is_object()
            ) {
                if (
                    replay.botInfo.name != "xdBot"
                )
                    rotation = false;

                auto const& p1 =
                    frameFixJson["p1"];

                if (p1.contains("x"))
                    frameFix.p1.pos.x =
                        p1["x"].get<float>();

                if (p1.contains("y"))
                    frameFix.p1.pos.y =
                        p1["y"].get<float>();

                if (
                    p1.contains("r") &&
                    rotation
                )
                    frameFix.p1.rotation =
                        p1["r"].get<float>();

                if (
                    frameFixJson.contains("p2") &&
                    frameFixJson["p2"].is_object()
                ) {
                    auto const& p2 =
                        frameFixJson["p2"];

                    if (p2.contains("x"))
                        frameFix.p2.pos.x =
                            p2["x"].get<float>();

                    if (p2.contains("y"))
                        frameFix.p2.pos.y =
                            p2["y"].get<float>();

                    if (
                        p2.contains("r") &&
                        rotation
                    )
                        frameFix.p2.rotation =
                            p2["r"].get<float>();
                }
            }

            else {
                continue;
            }

            replay.frameFixes.push_back(
                frameFix
            );
        }

        return replay;
    }

    std::vector<uint8_t> exportData(
        bool exportJson = false
    ) {
        json replayJson =
            saveExtension();

        replayJson["gameVersion"] =
            gameVersion;

        replayJson["description"] =
            description;

        replayJson["version"] =
            version;

        replayJson["duration"] =
            duration;

        replayJson["bot"]["name"] =
            botInfo.name;

        replayJson["bot"]["version"] =
            botInfo.version;

        replayJson["level"]["id"] =
            levelInfo.id;

        replayJson["level"]["name"] =
            levelInfo.name;

        replayJson["author"] =
            author;

        replayJson["seed"] =
            seed;

        replayJson["coins"] =
            coins;

        replayJson["ldm"] =
            ldm;

        replayJson["framerate"] =
            framerate;

        if (lastRecordedFrame > 0)
            replayJson["lastRecordedFrame"] =
                lastRecordedFrame;

        for (
            InputType const& input :
            inputs
        ) {
            json inputJson =
                input.saveExtension();

            inputJson["frame"] =
                input.frame;

            inputJson["btn"] =
                input.button;

            inputJson["2p"] =
                input.player2;

            inputJson["down"] =
                input.down;

            replayJson["inputs"].push_back(
                inputJson
            );
        }

        for (
            FrameFix const& frameFix :
            frameFixes
        ) {
            json frameFixJson;

            json p1Json;
            json p2Json;

            if (frameFix.p1.pos.x != 0.f)
                p1Json["x"] =
                    frameFix.p1.pos.x;

            if (frameFix.p1.pos.y != 0.f)
                p1Json["y"] =
                    frameFix.p1.pos.y;

            if (frameFix.p1.rotation != 0.f)
                p1Json["r"] =
                    frameFix.p1.rotation;

            if (frameFix.p2.pos.x != 0.f)
                p2Json["x"] =
                    frameFix.p2.pos.x;

            if (frameFix.p2.pos.y != 0.f)
                p2Json["y"] =
                    frameFix.p2.pos.y;

            if (frameFix.p2.rotation != 0.f)
                p2Json["r"] =
                    frameFix.p2.rotation;

            if (
                p1Json.empty() &&
                p2Json.empty()
            )
                continue;

            frameFixJson["frame"] =
                frameFix.frame;

            frameFixJson["p1"] =
                p1Json;

            if (!p2Json.empty())
                frameFixJson["p2"] =
                    p2Json;

            replayJson["frameFixes"].push_back(
                frameFixJson
            );
        }

        if (exportJson) {
            std::string replayString =
                replayJson.dump();

            return std::vector<uint8_t>(
                replayString.begin(),
                replayString.end()
            );
        }

        return json::to_msgpack(
            replayJson
        );
    }
};

} // namespace gdr