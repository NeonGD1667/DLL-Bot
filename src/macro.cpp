#include "includes.hpp"
#include "ui/game_ui.hpp"
#include "ui/record_layer.hpp"

#include <array>
#include <bit>
#include <cstring>
#include <optional>
#include <span>

#include <Geode/modify/PlayLayer.hpp>

namespace {

class GDR2Reader {
public:
  explicit GDR2Reader(std::span<std::uint8_t const> data) : m_data(data) {}

  bool empty() const { return m_data.empty(); }

  bool readBytes(void *out, size_t size) {
    if (size > m_data.size())
      return false;

    std::memcpy(out, m_data.data(), size);
    m_data = m_data.subspan(size);
    return true;
  }

  bool skip(size_t size) {
    if (size > m_data.size())
      return false;

    m_data = m_data.subspan(size);
    return true;
  }

  template <typename T> bool readVarint(T &out) {
    static_assert(std::is_integral_v<T>);

    uint64_t value = 0;
    int shift = 0;

    while (true) {
      if (m_data.empty() || shift > 63)
        return false;

      auto byte = m_data.front();
      m_data = m_data.subspan(1);

      value |= static_cast<uint64_t>(byte & 0x7F) << shift;
      if ((byte & 0x80) == 0)
        break;

      shift += 7;
    }

    out = static_cast<T>(value);
    return true;
  }

  template <typename T> bool readBE(T &out) {
    std::array<std::uint8_t, sizeof(T)> bytes{};
    if (!readBytes(bytes.data(), bytes.size()))
      return false;

    if constexpr (std::endian::native == std::endian::little)
      std::reverse(bytes.begin(), bytes.end());

    std::memcpy(&out, bytes.data(), sizeof(T));
    return true;
  }

  bool readBool(bool &out) {
    uint8_t value = 0;
    if (!readVarint(value))
      return false;
    out = value != 0;
    return true;
  }

  bool readString(std::string &out) {
    size_t size = 0;
    if (!readVarint(size))
      return false;
    if (size > m_data.size())
      return false;

    out.assign(reinterpret_cast<char const *>(m_data.data()), size);
    m_data = m_data.subspan(size);
    return true;
  }

private:
  std::span<std::uint8_t const> m_data;
};

// Đối xứng với GDR2Reader — cùng format varint/BE/string/bool để round-trip
// đúng với importGDR2.
class GDR2Writer {
public:
  void writeBytes(const void *data, size_t size) {
    const std::uint8_t *bytes = static_cast<const std::uint8_t *>(data);
    m_data.insert(m_data.end(), bytes, bytes + size);
  }

  template <typename T> void writeVarint(T value) {
    static_assert(std::is_integral_v<T>);
    uint64_t v = static_cast<uint64_t>(value);

    do {
      std::uint8_t byte = v & 0x7F;
      v >>= 7;
      if (v != 0)
        byte |= 0x80;
      m_data.push_back(byte);
    } while (v != 0);
  }

  template <typename T> void writeBE(T value) {
    std::array<std::uint8_t, sizeof(T)> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(T));

    if constexpr (std::endian::native == std::endian::little)
      std::reverse(bytes.begin(), bytes.end());

    writeBytes(bytes.data(), bytes.size());
  }

  void writeBool(bool value) { writeVarint<std::uint8_t>(value ? 1 : 0); }

  void writeString(std::string const &value) {
    writeVarint(value.size());
    writeBytes(value.data(), value.size());
  }

  std::vector<std::uint8_t> const &data() const { return m_data; }

private:
  std::vector<std::uint8_t> m_data;
};

} // namespace

void Macro::recordAction(int frame, int button, bool player2, bool hold) {
  PlayLayer *pl = PlayLayer::get();
  if (!pl)
    return;

  auto &g = Global::get();

  if (g.macro.inputs.empty())
    Macro::updateInfo(pl);

  if (g.tpsEnabled)
    g.macro.framerate = g.tps;

  if (Macro::flipControls())
    player2 = !player2;

  g.macro.inputs.push_back(input(frame, button, player2, hold));
}

void Macro::recordFrameFix(int frame, PlayerObject *p1, PlayerObject *p2) {
  float p1Rotation = p1->getRotation();
  float p2Rotation = p2->getRotation();

  while (p1Rotation < 0 || p1Rotation > 360)
    p1Rotation += p1Rotation < 0 ? 360.f : -360.f;

  while (p2Rotation < 0 || p2Rotation > 360)
    p2Rotation += p2Rotation < 0 ? 360.f : -360.f;

  Global::get().macro.frameFixes.push_back({frame,
                                            {p1->getPosition(), p1Rotation},
                                            {p2->getPosition(), p2Rotation}});
}

bool Macro::flipControls() {
  PlayLayer *pl = PlayLayer::get();
  if (!pl)
    return GameManager::get()->getGameVariable("0010");

  return pl->m_levelSettings->m_platformerMode
             ? false
             : GameManager::get()->getGameVariable("0010");
}

void Macro::autoSave(GJGameLevel *level, int number) {
  if (!level)
    level = PlayLayer::get() != nullptr ? PlayLayer::get()->m_level : nullptr;
  if (!level)
    return;

  std::string levelname = level->m_levelName;
  std::filesystem::path autoSavesPath =
      Mod::get()->getSettingValue<std::filesystem::path>("autosaves_folder");
  std::filesystem::path path =
      autoSavesPath / fmt::format("autosave_{}_{}", levelname, number);

  if (!std::filesystem::exists(autoSavesPath))
    return;

  std::string username = GJAccountManager::sharedState() != nullptr
                             ? GJAccountManager::sharedState()->m_username
                             : "";
  int result = Macro::save(
      username, fmt::format("AutoSave {} in level {}", number, levelname),
      path.string());

  if (result != 0)
    log::debug("Failed to autosave macro. ID: {}. Path: {}", result, path);
}

void Macro::tryAutosave(GJGameLevel *level, CheckpointObject *cp) {
  auto &g = Global::get();

  if (g.state != state::recording)
    return;
  if (!g.autosaveEnabled)
    return;
  if (!g.checkpoints.contains(cp))
    return;
  if (g.checkpoints[cp].frame < g.lastAutoSaveFrame)
    return;

  std::filesystem::path autoSavesPath =
      g.mod->getSettingValue<std::filesystem::path>("autosaves_folder");

  if (!std::filesystem::exists(autoSavesPath))
    return log::debug("Failed to access auto saves path.");

  std::string levelname = level->m_levelName;
  std::filesystem::path path =
      autoSavesPath /
      fmt::format("autosave_{}_{}", levelname, g.currentSession);
  std::error_code ec;
  std::filesystem::remove(path.string() + ".gdr", ec); // Remove previous save
  if (ec)
    log::warn("Failed to remove previous autosave");

  autoSave(level, g.currentSession);
}

void Macro::updateInfo(PlayLayer *pl) {
  if (!pl)
    return;

  auto &g = Global::get();

  GJGameLevel *level = pl->m_level;
  if (level->m_lowDetailModeToggled != g.macro.ldm)
    g.macro.ldm = level->m_lowDetailModeToggled;

  int id = level->m_levelID.value();
  if (id != g.macro.levelInfo.id)
    g.macro.levelInfo.id = id;

  bool platformer = pl->m_levelSettings->m_platformerMode;
  if (platformer != g.macro.platformer)
    g.macro.platformer = platformer;

  std::string name = level->m_levelName;
  if (name != g.macro.levelInfo.name)
    g.macro.levelInfo.name = name;

  std::string author = GJAccountManager::sharedState()->m_username;
  if (g.macro.author != author)
    g.macro.author = author;

  if (g.macro.author == "")
    g.macro.author = "N/A";

  g.macro.botInfo.name = "xdBot";
  g.macro.botInfo.version = xdBotVersion;
  g.macro.xdBotMacro = true;
}

void Macro::updateTPS() {
  auto &g = Global::get();

  if (g.state != state::none && !g.macro.inputs.empty()) {
    g.previousTpsEnabled = g.tpsEnabled;
    g.previousTps = g.tps;

    g.tpsEnabled = g.macro.framerate != 240.f;
    if (g.tpsEnabled)
      g.tps = g.macro.framerate;

    g.mod->setSavedValue("macro_tps", g.tps);
    g.mod->setSavedValue("macro_tps_enabled", g.tpsEnabled);

  } else if (g.previousTps != 0.f) {
    g.tpsEnabled = g.previousTpsEnabled;
    g.tps = g.previousTps;
    g.previousTps = 0.f;
    g.mod->setSavedValue("macro_tps", g.tps);
    g.mod->setSavedValue("macro_tps_enabled", g.tpsEnabled);
  }

  if (g.layer)
    static_cast<RecordLayer *>(g.layer)->updateTPS();
}

int Macro::save(std::string author, std::string desc, std::string path,
                bool json) {
  auto &g = Global::get();

  if (g.macro.inputs.empty())
    return 31;

  std::string extension = json ? ".gdr.json" : ".gdr";

  int iterations = 0;

  while (std::filesystem::exists(path + extension)) {
    iterations++;

    if (iterations > 1) {
      int length = 3 + std::to_string(iterations - 1).length();
      path.erase(path.length() - length, length);
    }

    path += fmt::format(" ({})", std::to_string(iterations));
  }

  path += extension;

  log::debug("Saving macro to path: {}", path);

  g.macro.author = author;
  g.macro.description = desc;
  g.macro.duration = g.macro.inputs.back().frame / g.macro.framerate;

  // NakoMod: Save last recorded frame for Continue Botting
  g.macro.lastRecordedFrame = g.macro.inputs.back().frame;

#ifdef GEODE_IS_WINDOWS
  std::wstring widePath = Utils::widen(path);

  if (widePath == L"Widen Error")
    return 30;

  std::ofstream f(widePath, std::ios::binary);
#else
  std::ofstream f(path, std::ios::binary);
#endif

  if (!f)
    f.open(path, std::ios::binary);

  if (!f)
    return 20;

  std::vector<gdr::FrameFix> frameFixes = g.macro.frameFixes;

  auto data = g.macro.exportData(json);

  f.write(reinterpret_cast<const char *>(data.data()), data.size());

  if (!f) {
    f.close();
    return 21;
  }

  if (!f)
    return 22;

  f.close();

  return 0;
}

// Lưu macro ra định dạng .gdr2 — song song với save(), không đụng code cũ.
int Macro::saveGDR2(std::string author, std::string desc, std::string path) {
  auto &g = Global::get();

  if (g.macro.inputs.empty())
    return 31;

  std::string extension = ".gdr2";

  int iterations = 0;

  while (std::filesystem::exists(path + extension)) {
    iterations++;

    if (iterations > 1) {
      int length = 3 + std::to_string(iterations - 1).length();
      path.erase(path.length() - length, length);
    }

    path += fmt::format(" ({})", std::to_string(iterations));
  }

  path += extension;

  log::debug("Saving GDR2 macro to path: {}", path);

  g.macro.author = author;
  g.macro.description = desc;
  g.macro.duration = g.macro.inputs.back().frame / g.macro.framerate;

  // NakoMod: Save last recorded frame for Continue Botting
  g.macro.lastRecordedFrame = g.macro.inputs.back().frame;

#ifdef GEODE_IS_WINDOWS
  std::wstring widePath = Utils::widen(path);

  if (widePath == L"Widen Error")
    return 30;

  std::ofstream f(widePath, std::ios::binary);
#else
  std::ofstream f(path, std::ios::binary);
#endif

  if (!f)
    f.open(path, std::ios::binary);

  if (!f)
    return 20;

  std::vector<std::uint8_t> data = g.macro.exportGDR2();

  f.write(reinterpret_cast<const char *>(data.data()), data.size());

  if (!f) {
    f.close();
    return 21;
  }

  f.close();

  return 0;
}

bool Macro::loadXDFile(std::filesystem::path path) {

  Macro newMacro = Macro::XDtoGDR(path);
  if (newMacro.description == "fail")
    return false;

  Global::get().macro = newMacro;
  return true;
}

Macro Macro::XDtoGDR(std::filesystem::path path) {

  Macro newMacro;
  newMacro.author = "N/A";
  newMacro.description = "N/A";
  newMacro.gameVersion = GEODE_GD_VERSION;

#ifdef GEODE_IS_WINDOWS
  std::ifstream file(Utils::widen(path.string()));
#else
  std::ifstream file(path.string());
#endif
  std::string line;

  if (!file.is_open()) {
    newMacro.description = "fail";
    return newMacro;
  }

  bool firstIt = true;
  bool andr = false;

  float fpsMultiplier = 1.f;

  while (std::getline(file, line)) {
    std::string item;
    std::stringstream ss(line);
    std::vector<std::string> action;

    while (std::getline(ss, item, '|'))
      action.push_back(item);

    if (action.size() < 4) {
      if (action[0] == "android")
        fpsMultiplier = 4.f;
      else {
        int fps = std::stoi(action[0]);
        fpsMultiplier = 240.f / fps;
      }

      continue;
    }

    int frame = static_cast<int>(round(std::stoi(action[0]) * fpsMultiplier));
    int button = std::stoi(action[2]);
    bool hold = action[1] == "1";
    bool player2 = action[3] == "1";
    bool posOnly = action[4] == "1";

    if (!posOnly)
      newMacro.inputs.push_back(input(frame, button, player2, hold));
    else {
      cocos2d::CCPoint p1Pos = ccp(std::stof(action[5]), std::stof(action[6]));
      cocos2d::CCPoint p2Pos =
          ccp(std::stof(action[11]), std::stof(action[12]));

      newMacro.frameFixes.push_back(
          {frame, {p1Pos, 0.f, false}, {p2Pos, 0.f, false}});
    }
  }

  file.close();

  return newMacro;
}

bool Macro::isGDR2Data(std::vector<std::uint8_t> const &data) {
  return data.size() >= 4 && data[0] == 'G' && data[1] == 'D' &&
         data[2] == 'R';
}

std::optional<Macro> Macro::importGDR2(
    std::vector<std::uint8_t> const &data) {
  if (!isGDR2Data(data))
    return std::nullopt;

  GDR2Reader reader(data);

  char magic[3]{};
  if (!reader.readBytes(magic, sizeof(magic)))
    return std::nullopt;

  uint64_t version = 0;
  if (!reader.readVarint(version) || version != 2)
    return std::nullopt;

  Macro macro;
  std::string inputTag;

  int gameVersion = 0;
  double framerate = 240.0;
  int replaySeed = 0;

  if (!reader.readString(inputTag) || !reader.readString(macro.author) ||
      !reader.readString(macro.description) || !reader.readBE(macro.duration) ||
      !reader.readVarint(gameVersion) || !reader.readBE(framerate) ||
      !reader.readVarint(replaySeed) || !reader.readVarint(macro.coins) ||
      !reader.readBool(macro.ldm))
    return std::nullopt;

  macro.gameVersion = static_cast<float>(gameVersion);
  macro.framerate = static_cast<float>(framerate);
  static_cast<gdr::Replay<Macro, input> &>(macro).seed = replaySeed;
  macro.seed = static_cast<uintptr_t>(replaySeed);

  bool platformer = false;
  if (!reader.readBool(platformer) || !reader.readString(macro.botInfo.name))
    return std::nullopt;

  macro.platformer = platformer;

  int botVersion = 0;
  if (!reader.readVarint(botVersion) || !reader.readVarint(macro.levelInfo.id) ||
      !reader.readString(macro.levelInfo.name))
    return std::nullopt;

  macro.botInfo.version = std::to_string(botVersion);

  size_t extensionSize = 0;
  if (!reader.readVarint(extensionSize) || !reader.skip(extensionSize))
    return std::nullopt;

  size_t deathCount = 0;
  if (!reader.readVarint(deathCount))
    return std::nullopt;

  uint64_t deathFrame = 0;
  for (size_t i = 0; i < deathCount; ++i) {
    uint64_t delta = 0;
    if (!reader.readVarint(delta))
      return std::nullopt;
    deathFrame += delta;
  }

  size_t inputCount = 0;
  size_t p1InputCount = 0;
  if (!reader.readVarint(inputCount) || !reader.readVarint(p1InputCount) ||
      p1InputCount > inputCount)
    return std::nullopt;

  macro.inputs.reserve(inputCount);

  uint64_t p1Frame = 0;
  uint64_t p2Frame = 0;
  bool hasInputExtensions = !inputTag.empty();

  for (size_t i = 0; i < inputCount; ++i) {
    uint64_t packed = 0;
    if (!reader.readVarint(packed))
      return std::nullopt;

    bool player2 = i >= p1InputCount;
    uint64_t &frameBase = player2 ? p2Frame : p1Frame;

    uint64_t delta = platformer ? (packed >> 3) : (packed >> 1);
    int button = platformer ? static_cast<int>((packed >> 1) & 0b11) : 1;
    bool down = (packed & 1) != 0;

    frameBase += delta;
    macro.inputs.emplace_back(static_cast<int>(frameBase), button, player2,
                              down);

    if (hasInputExtensions) {
      size_t inputExtensionSize = 0;
      if (!reader.readVarint(inputExtensionSize) ||
          !reader.skip(inputExtensionSize))
        return std::nullopt;
    }
  }

  macro.lastRecordedFrame =
      macro.inputs.empty() ? 0 : static_cast<int>(macro.inputs.back().frame);
  macro.xdBotMacro = macro.botInfo.name == "xdBot";

  return macro;
}

// Đối xứng với importGDR2 — export instance hiện tại ra bytes .gdr2.
std::vector<std::uint8_t> Macro::exportGDR2() {
  GDR2Writer writer;

  // magic + version — phải khớp check trong isGDR2Data/importGDR2
  writer.writeBytes("GDR", 3);
  writer.writeVarint<uint64_t>(2);

  // Bot hiện không dùng input extension riêng -> tag rỗng
  std::string inputTag = "";
  writer.writeString(inputTag);

  writer.writeString(author);
  writer.writeString(description);
  writer.writeBE(duration);

  writer.writeVarint(static_cast<int>(gameVersion));
  writer.writeBE(static_cast<double>(framerate));

  int replaySeed = static_cast<gdr::Replay<Macro, input> &>(*this).seed;
  writer.writeVarint(replaySeed);

  writer.writeVarint(coins);
  writer.writeBool(ldm);
  writer.writeBool(this->platformer);

  writer.writeString(botInfo.name);

  int botVersion = 0;
  try {
    botVersion = std::stoi(botInfo.version);
  } catch (...) {
    botVersion = 0;
  }
  writer.writeVarint(botVersion);

  writer.writeVarint(levelInfo.id);
  writer.writeString(levelInfo.name);

  // Extension rỗng — chưa dùng
  writer.writeVarint<size_t>(0);

  // Deaths — chưa track trong struct hiện tại, ghi 0
  writer.writeVarint<size_t>(0);

  // Sắp lại: player 1 trước, player 2 sau (giống format đọc bởi importGDR2)
  std::vector<input const *> p1Inputs;
  std::vector<input const *> p2Inputs;
  for (auto const &in : inputs) {
    if (in.player2)
      p2Inputs.push_back(&in);
    else
      p1Inputs.push_back(&in);
  }

  writer.writeVarint(inputs.size());
  writer.writeVarint(p1Inputs.size());

  auto writeGroup = [&](std::vector<input const *> const &group) {
    uint64_t prevFrame = 0;
    for (auto const *in : group) {
      uint64_t delta = static_cast<uint64_t>(in->frame) - prevFrame;
      prevFrame = static_cast<uint64_t>(in->frame);

      uint64_t packed;
      if (this->platformer)
        packed = (delta << 3) | ((static_cast<uint64_t>(in->button) & 0b11) << 1) | (in->down ? 1 : 0);
      else
        packed = (delta << 1) | (in->down ? 1 : 0);

      writer.writeVarint(packed);
      // inputTag rỗng -> không ghi input extension
    }
  };

  writeGroup(p1Inputs);
  writeGroup(p2Inputs);

  return writer.data();
}

void Macro::resetVariables() {
  auto &g = Global::get();

  g.ignoreFrame = -1;
  g.ignoreJumpButton = -1;

  g.delayedFrameReleaseMain[0] = -1;
  g.delayedFrameReleaseMain[1] = -1;

  g.delayedFrameInput[0] = -1;
  g.delayedFrameInput[1] = -1;

  g.addSideHoldingMembers[0] = false;
  g.addSideHoldingMembers[1] = false;
  for (int x = 0; x < 2; x++) {
    for (int y = 0; y < 2; y++)
      g.delayedFrameRelease[x][y] = -1;
  }
}

void Macro::resetState(bool cp) {
  auto &g = Global::get();

  g.restart = false;
  g.state = state::none;

  if (!cp)
    g.checkpoints.clear();

  Interface::updateLabels();
  Interface::updateButtons();

  Macro::resetVariables();
}

void Macro::togglePlaying() {
  if (Global::hasIncompatibleMods())
    return;

  auto &g = Global::get();

  if (g.layer) {
    static_cast<RecordLayer *>(g.layer)->playing->toggle(Global::get().state !=
                                                         state::playing);
    static_cast<RecordLayer *>(g.layer)->togglePlaying(nullptr);
  } else {
    RecordLayer *layer = RecordLayer::create();
    layer->togglePlaying(nullptr);
    layer->onClose(nullptr);
  }
}

void Macro::toggleRecording() {
  if (Global::hasIncompatibleMods())
    return;

  auto &g = Global::get();

  if (g.layer) {
    static_cast<RecordLayer *>(g.layer)->recording->toggle(
        Global::get().state != state::recording);
    static_cast<RecordLayer *>(g.layer)->toggleRecording(nullptr);
  } else {
    RecordLayer *layer = RecordLayer::create();
    layer->toggleRecording(nullptr);
    layer->onClose(nullptr);
  }
}

bool Macro::shouldStep() {
  auto &g = Global::get();

  if (g.stepFrame)
    return true;
  if (Global::getCurrentFrame() == 0)
    return true;

  // if (g.ignoreFrame != -1) return true;
  // if (g.ignoreJumpButton != -1) return true;

  // if (g.delayedFrameReleaseMain[0] != -1) return true;
  // if (g.delayedFrameReleaseMain[1] != -1) return true;

  // if (g.delayedFrameInput[0] != -1) return true;
  // if (g.delayedFrameInput[1] != -1) return true;

  // for (int x = 0; x < 2; x++) {
  //     for (int y = 0; y < 2; y++) {
  //         if (g.delayedFrameRelease[x][y] != -1) return true;
  //     }
  // }

  return false;
}
