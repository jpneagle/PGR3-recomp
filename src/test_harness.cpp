// Unattended testing support, driven by environment variables:
//   PGR3_AUTOPRESS="start@5000-5300,a@9000-9200,lt@...,ly-@..."  scripted pad (times in ms from
//                  launch). Replaces all real input devices with one virtual pad.
//   PGR3_DUMP_FRAMES=<ms>   save the guest output every <ms> to PGR3_DUMP_DIR (default "frames")
//                           as frames/<ms since launch>.bmp

#include "test_harness.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <rex/input/input_driver.h>
#include <rex/input/input_system.h>
#include <rex/input/nop/nop_input_driver.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/interfaces/graphics.h>
#include <rex/ui/presenter.h>

namespace pgr3 {
namespace {

using rex::X_RESULT;
using rex::X_STATUS;

using Clock = std::chrono::steady_clock;
const Clock::time_point g_start = Clock::now();

uint32_t NowMs() {
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_start)
                      .count());
}

struct Press {
  std::string control;
  uint32_t from, to;
};

std::vector<Press> ParseScript(const char* s) {
  std::vector<Press> out;
  std::string all(s);
  size_t pos = 0;
  while (pos < all.size()) {
    size_t comma = all.find(',', pos);
    std::string item = all.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
    pos = comma == std::string::npos ? all.size() : comma + 1;
    size_t at = item.find('@'), dash = item.find('-', at);
    if (at == std::string::npos || dash == std::string::npos) continue;
    out.push_back({item.substr(0, at), uint32_t(std::stoul(item.substr(at + 1, dash - at - 1))),
                   uint32_t(std::stoul(item.substr(dash + 1)))});
  }
  return out;
}

constexpr rex::input::DeviceId kScriptDevice = static_cast<rex::input::DeviceId>(0x50475233);

class ScriptedPadDriver final : public rex::input::InputDriver {
 public:
  explicit ScriptedPadDriver(std::vector<Press> script)
      : InputDriver(nullptr, 0), script_(std::move(script)) {}

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }

  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override {
    rex::input::DeviceInfo info;
    info.id = kScriptDevice;
    info.name = "Scripted pad";
    out.push_back(info);
  }

  X_RESULT GetDeviceState(rex::input::DeviceId id, rex::input::X_INPUT_STATE* st) override {
    if (id != kScriptDevice) return X_ERROR_DEVICE_NOT_CONNECTED;
    uint32_t now = NowMs();
    uint16_t buttons = 0;
    uint8_t lt = 0, rt = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
    for (const auto& p : script_) {
      if (now < p.from || now >= p.to) continue;
      const std::string& c = p.control;
      if (c == "up") buttons |= 0x0001;
      else if (c == "down") buttons |= 0x0002;
      else if (c == "left") buttons |= 0x0004;
      else if (c == "right") buttons |= 0x0008;
      else if (c == "start") buttons |= 0x0010;
      else if (c == "back") buttons |= 0x0020;
      else if (c == "lb") buttons |= 0x0100;
      else if (c == "rb") buttons |= 0x0200;
      else if (c == "a") buttons |= 0x1000;
      else if (c == "b") buttons |= 0x2000;
      else if (c == "x") buttons |= 0x4000;
      else if (c == "y") buttons |= 0x8000;
      else if (c == "lt") lt = 255;
      else if (c == "rt") rt = 255;
      else if (c == "lx-") lx = -32767;
      else if (c == "lx+") lx = 32767;
      else if (c == "ly-") ly = -32767;
      else if (c == "ly+") ly = 32767;
      else if (c == "rx-") rx = -32767;
      else if (c == "rx+") rx = 32767;
      else if (c == "ry-") ry = -32767;
      else if (c == "ry+") ry = 32767;
    }
    uint64_t key = uint64_t(buttons) | uint64_t(lt) << 16 | uint64_t(rt) << 24 |
                   uint64_t(uint16_t(lx)) << 32 | uint64_t(uint16_t(ly)) << 48;
    if (key != last_key_) {
      last_key_ = key;
      ++packet_;
    }
    if (st) {
      std::memset(st, 0, sizeof(*st));
      st->packet_number = packet_;
      st->gamepad.buttons = buttons;
      st->gamepad.left_trigger = lt;
      st->gamepad.right_trigger = rt;
      st->gamepad.thumb_lx = lx;
      st->gamepad.thumb_ly = ly;
      st->gamepad.thumb_rx = rx;
      st->gamepad.thumb_ry = ry;
    }
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetDeviceCapabilities(rex::input::DeviceId id, uint32_t,
                                 rex::input::X_INPUT_CAPABILITIES* caps) override {
    if (id != kScriptDevice) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (caps) {
      std::memset(caps, 0, sizeof(*caps));
      caps->type = 0x01;
      caps->sub_type = 0x01;
      caps->gamepad.buttons = 0xFFFF;
      caps->gamepad.left_trigger = 0xFF;
      caps->gamepad.right_trigger = 0xFF;
    }
    return X_ERROR_SUCCESS;
  }

  X_RESULT SetDeviceVibration(rex::input::DeviceId id, rex::input::X_INPUT_VIBRATION*) override {
    return id == kScriptDevice ? X_ERROR_SUCCESS : X_ERROR_DEVICE_NOT_CONNECTED;
  }

  X_RESULT GetDeviceKeystroke(rex::input::DeviceId id, uint32_t,
                              rex::input::X_INPUT_KEYSTROKE*) override {
    return id == kScriptDevice ? X_ERROR_EMPTY : X_ERROR_DEVICE_NOT_CONNECTED;
  }

 private:
  std::vector<Press> script_;
  uint64_t last_key_ = 0;
  uint32_t packet_ = 1;
};

bool WriteBmp(const std::filesystem::path& path, const rex::ui::RawImage& img) {
  FILE* f = _wfopen(path.c_str(), L"wb");
  if (!f) return false;
  uint32_t row = img.width * 3, pad = (4 - row % 4) % 4, size = (row + pad) * img.height;
  uint8_t hdr[54] = {'B', 'M'};
  auto put32 = [&](int o, uint32_t v) { std::memcpy(hdr + o, &v, 4); };
  put32(2, 54 + size);
  put32(10, 54);
  put32(14, 40);
  put32(18, img.width);
  put32(22, img.height);
  hdr[26] = 1;
  hdr[28] = 24;
  put32(34, size);
  fwrite(hdr, 1, 54, f);
  std::vector<uint8_t> line(row + pad);
  for (int y = int(img.height) - 1; y >= 0; --y) {
    const uint8_t* src = img.data.data() + size_t(y) * img.stride;
    for (uint32_t x = 0; x < img.width; ++x) {
      line[x * 3 + 0] = src[x * 4 + 2];
      line[x * 3 + 1] = src[x * 4 + 1];
      line[x * 3 + 2] = src[x * 4 + 0];
    }
    fwrite(line.data(), 1, line.size(), f);
  }
  fclose(f);
  return true;
}

std::atomic<bool> g_stop{false};
std::thread g_dumper;

}  // namespace

void ConfigureTestInput(rex::RuntimeConfig& config) {
  const char* script = std::getenv("PGR3_AUTOPRESS");
  if (!script || !*script) return;
  auto presses = ParseScript(script);
  REXLOG_INFO("test harness: scripted pad with {} presses", presses.size());
  config.input_factory = [presses](bool) -> std::unique_ptr<rex::system::IInputSystem> {
    auto input = std::make_unique<rex::input::InputSystem>(nullptr);
    input->AddDriver(std::make_unique<ScriptedPadDriver>(presses));
    input->SetDeviceAssignment(std::make_unique<rex::input::SlotAssignment>());
    return input;
  };
}

void StartFrameDumper(rex::Runtime* runtime) {
  const char* interval = std::getenv("PGR3_DUMP_FRAMES");
  if (!interval || !*interval || !runtime) return;
  uint32_t ms = uint32_t(std::strtoul(interval, nullptr, 10));
  if (!ms) return;
  const char* dir_env = std::getenv("PGR3_DUMP_DIR");
  std::filesystem::path dir = dir_env && *dir_env ? dir_env : "frames";
  std::filesystem::create_directories(dir);
  REXLOG_INFO("test harness: dumping frames every {} ms to {}", ms, dir.string());
  g_dumper = std::thread([runtime, ms, dir] {
    uint32_t next = ms;
    while (!g_stop) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      uint32_t now = NowMs();
      if (now < next) continue;
      next = now + ms;
      auto* gfx = runtime->graphics_system();
      auto* presenter = gfx ? gfx->presenter() : nullptr;
      rex::ui::RawImage img;
      if (presenter && presenter->CaptureGuestOutput(img) && img.width) {
        char name[32];
        snprintf(name, sizeof(name), "%07u.bmp", now);
        WriteBmp(dir / name, img);
      }
    }
  });
}

void StopTestHarness() {
  g_stop = true;
  if (g_dumper.joinable()) g_dumper.join();
}

}  // namespace pgr3
