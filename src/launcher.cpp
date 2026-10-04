// Startup settings window: game folder, language, rendering resolution and display mode.
// Choices are saved to pgr3_launcher.toml next to the exe and loaded as a cvar config file, so
// anything given on the command line still wins.

#include "launcher.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(show_launcher, true, "PGR3",
                    "Show the settings window at startup (hold Shift to force it)");

namespace pgr3 {
namespace {

struct Language {
  uint32_t id;
  const wchar_t* name;
};
// XLanguage ids for the eight languages on the disc (UI/Text/*.ini).
const Language kLanguages[] = {
    {2, L"日本語"},   {1, L"English"},  {4, L"Français"}, {3, L"Deutsch"},
    {5, L"Español"},  {6, L"Italiano"}, {7, L"한국어"},   {8, L"繁體中文"},
};

struct Resolution {
  uint32_t width, height, scale;
  const wchar_t* name;
};
// The guest outputs 1280x720; resolution_scale multiplies every render target.
const Resolution kResolutions[] = {
    {1280, 720, 1, L"1280 × 720 (720p, 等倍)"},
    {1920, 1080, 2, L"1920 × 1080 (1080p)"},
    {2560, 1440, 2, L"2560 × 1440 (1440p)"},
    {3840, 2160, 3, L"3840 × 2160 (4K)"},
};

struct Settings {
  std::wstring game_dir;
  uint32_t language = 2;
  uint32_t width = 1920, height = 1080;
  bool fullscreen = true;
  bool show = true;
};

std::wstring Widen(const std::string& s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
  return w;
}

std::string Narrow(const std::wstring& w) {
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

std::string TomlString(const std::string& s) {
  std::string out = "'";  // literal string: backslashes in paths stay as-is
  out += s;
  out += "'";
  return out;
}

Settings Load(const std::filesystem::path& path) {
  Settings s;
  std::ifstream in(path);
  std::map<std::string, std::string> kv;
  for (std::string line; std::getline(in, line);) {
    size_t eq = line.find('=');
    if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
    auto trim = [](std::string v) {
      size_t a = v.find_first_not_of(" \t\r"), b = v.find_last_not_of(" \t\r");
      v = a == std::string::npos ? "" : v.substr(a, b - a + 1);
      if (v.size() >= 2 && (v[0] == '\'' || v[0] == '"')) v = v.substr(1, v.size() - 2);
      return v;
    };
    kv[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
  }
  auto num = [&](const char* k, uint32_t def) {
    auto it = kv.find(k);
    return it == kv.end() ? def : uint32_t(std::strtoul(it->second.c_str(), nullptr, 10));
  };
  if (kv.count("game_data_root")) s.game_dir = Widen(kv["game_data_root"]);
  s.language = num("user_language", s.language);
  s.width = num("output_width", s.width);
  s.height = num("output_height", s.height);
  if (kv.count("fullscreen")) s.fullscreen = kv["fullscreen"] == "true";
  if (kv.count("show_launcher")) s.show = kv["show_launcher"] != "false";
  return s;
}

const Resolution& ResolutionFor(const Settings& s) {
  for (const auto& r : kResolutions)
    if (r.width == s.width && r.height == s.height) return r;
  return kResolutions[1];
}

void Save(const std::filesystem::path& path, const Settings& s) {
  const Resolution& r = ResolutionFor(s);
  // Windowed mode: the requested size, shrunk to fit the desktop work area.
  RECT work{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  uint32_t ww = r.width, wh = r.height;
  uint32_t max_w = uint32_t(work.right - work.left) * 9 / 10;
  uint32_t max_h = uint32_t(work.bottom - work.top) * 9 / 10;
  while (ww > max_w || wh > max_h) {
    ww = ww * 3 / 4;
    wh = wh * 3 / 4;
  }
  std::ofstream out(path, std::ios::trunc);
  out << "# Written by the PGR3 startup settings window.\n";
  out << "game_data_root = " << TomlString(Narrow(s.game_dir)) << "\n";
  out << "user_language = " << s.language << "\n";
  out << "resolution_scale = " << r.scale << "\n";
  out << "fullscreen = " << (s.fullscreen ? "true" : "false") << "\n";
  out << "window_width = " << ww << "\n";
  out << "window_height = " << wh << "\n";
  out << "show_launcher = " << (s.show ? "true" : "false") << "\n";
  out << "# Launcher state (not cvars)\n";
  out << "# pgr3_output_width = " << r.width << "\n";
  out << "# pgr3_output_height = " << r.height << "\n";
  // Keep the chosen preset readable on the next start.
  out << "[pgr3]\noutput_width = " << r.width << "\noutput_height = " << r.height << "\n";
}

// ---- window ----

enum : int {
  kIdGameDir = 100,
  kIdBrowse,
  kIdLanguage,
  kIdResolution,
  kIdFullscreen,
  kIdHideNext,
  kIdStart = IDOK,
  kIdQuit = IDCANCEL,
};

struct DialogState {
  Settings settings;
  bool accepted = false;
  bool done = false;
  HFONT font = nullptr;
  int dpi = 96;
};

int Px(const DialogState& st, int v) { return MulDiv(v, st.dpi, 96); }

bool HasGame(const std::wstring& dir) {
  return !dir.empty() && GetFileAttributesW((dir + L"\\default.xex").c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring BrowseFolder(HWND owner, const std::wstring& start) {
  std::wstring result;
  IFileOpenDialog* dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&dlg))))
    return result;
  DWORD opts = 0;
  dlg->GetOptions(&opts);
  dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
  dlg->SetTitle(L"ゲームフォルダ (default.xex のあるフォルダ) を選択");
  IShellItem* folder = nullptr;
  if (!start.empty() &&
      SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
    dlg->SetFolder(folder);
    folder->Release();
  }
  if (SUCCEEDED(dlg->Show(owner))) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dlg->GetResult(&item))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        result = path;
        CoTaskMemFree(path);
      }
      item->Release();
    }
  }
  dlg->Release();
  return result;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  auto* st = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_CREATE: {
      st = reinterpret_cast<DialogState*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, LONG_PTR(st));
      auto add = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w,
                     int h, int id) {
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, Px(*st, x),
                                 Px(*st, y), Px(*st, w), Px(*st, h), hwnd, HMENU(INT_PTR(id)),
                                 nullptr, nullptr);
        SendMessageW(c, WM_SETFONT, WPARAM(st->font), TRUE);
        return c;
      };
      const Settings& s = st->settings;
      add(L"STATIC", L"ゲームフォルダ", 0, 16, 18, 110, 20, -1);
      add(L"EDIT", s.game_dir.c_str(), WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 130, 15, 270, 24,
          kIdGameDir);
      add(L"BUTTON", L"参照...", WS_TABSTOP | BS_PUSHBUTTON, 406, 14, 70, 26, kIdBrowse);

      add(L"STATIC", L"言語 / Language", 0, 16, 56, 110, 20, -1);
      HWND lang = add(L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 130, 52, 200,
                      300, kIdLanguage);
      for (int i = 0; i < int(std::size(kLanguages)); ++i) {
        SendMessageW(lang, CB_ADDSTRING, 0, LPARAM(kLanguages[i].name));
        if (kLanguages[i].id == s.language) SendMessageW(lang, CB_SETCURSEL, i, 0);
      }
      if (SendMessageW(lang, CB_GETCURSEL, 0, 0) == CB_ERR) SendMessageW(lang, CB_SETCURSEL, 0, 0);

      add(L"STATIC", L"解像度", 0, 16, 94, 110, 20, -1);
      HWND res = add(L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 130, 90, 260,
                     300, kIdResolution);
      const Resolution& cur = ResolutionFor(s);
      for (int i = 0; i < int(std::size(kResolutions)); ++i) {
        SendMessageW(res, CB_ADDSTRING, 0, LPARAM(kResolutions[i].name));
        if (&kResolutions[i] == &cur) SendMessageW(res, CB_SETCURSEL, i, 0);
      }

      HWND fs = add(L"BUTTON", L"フルスクリーン", WS_TABSTOP | BS_AUTOCHECKBOX, 130, 126, 250,
                    22, kIdFullscreen);
      SendMessageW(fs, BM_SETCHECK, s.fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);
      HWND hide = add(L"BUTTON", L"次回からこの画面を表示しない (Shift を押しながら起動で表示)",
                      WS_TABSTOP | BS_AUTOCHECKBOX, 130, 152, 346, 22, kIdHideNext);
      SendMessageW(hide, BM_SETCHECK, s.show ? BST_UNCHECKED : BST_CHECKED, 0);

      add(L"BUTTON", L"開始", WS_TABSTOP | BS_DEFPUSHBUTTON, 290, 190, 90, 30, kIdStart);
      add(L"BUTTON", L"終了", WS_TABSTOP | BS_PUSHBUTTON, 386, 190, 90, 30, kIdQuit);
      return 0;
    }
    case WM_COMMAND:
      switch (LOWORD(wp)) {
        case kIdBrowse: {
          wchar_t buf[MAX_PATH * 2] = {};
          GetDlgItemTextW(hwnd, kIdGameDir, buf, int(std::size(buf)));
          std::wstring dir = BrowseFolder(hwnd, buf);
          if (!dir.empty()) SetDlgItemTextW(hwnd, kIdGameDir, dir.c_str());
          return 0;
        }
        case kIdStart: {
          Settings& s = st->settings;
          wchar_t buf[MAX_PATH * 2] = {};
          GetDlgItemTextW(hwnd, kIdGameDir, buf, int(std::size(buf)));
          s.game_dir = buf;
          if (!HasGame(s.game_dir)) {
            MessageBoxW(hwnd,
                        L"default.xex が見つかりません。\nディスクから取り出したゲームフォルダを"
                        L"指定してください。",
                        L"PGR3", MB_OK | MB_ICONWARNING);
            return 0;
          }
          int li = int(SendDlgItemMessageW(hwnd, kIdLanguage, CB_GETCURSEL, 0, 0));
          int ri = int(SendDlgItemMessageW(hwnd, kIdResolution, CB_GETCURSEL, 0, 0));
          s.language = kLanguages[li < 0 ? 0 : li].id;
          s.width = kResolutions[ri < 0 ? 1 : ri].width;
          s.height = kResolutions[ri < 0 ? 1 : ri].height;
          s.fullscreen = IsDlgButtonChecked(hwnd, kIdFullscreen) == BST_CHECKED;
          s.show = IsDlgButtonChecked(hwnd, kIdHideNext) != BST_CHECKED;
          st->accepted = true;
          DestroyWindow(hwnd);
          return 0;
        }
        case kIdQuit:
          DestroyWindow(hwnd);
          return 0;
      }
      break;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      st->done = true;
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

bool RunDialog(Settings& settings) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&icc);

  DialogState st;
  st.settings = settings;
  HDC screen = GetDC(nullptr);
  st.dpi = GetDeviceCaps(screen, LOGPIXELSY);
  ReleaseDC(nullptr, screen);
  st.font = CreateFontW(-Px(st, 13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                        CLEARTYPE_QUALITY, 0, L"Yu Gothic UI");

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  wc.hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
  wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
  wc.lpszClassName = L"Pgr3Launcher";
  RegisterClassW(&wc);

  DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  RECT rc{0, 0, Px(st, 492), Px(st, 236)};
  AdjustWindowRect(&rc, style, FALSE);
  int w = rc.right - rc.left, h = rc.bottom - rc.top;
  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Project Gotham Racing 3 - 設定", style,
                              (GetSystemMetrics(SM_CXSCREEN) - w) / 2,
                              (GetSystemMetrics(SM_CYSCREEN) - h) / 2, w, h, nullptr, nullptr,
                              wc.hInstance, &st);
  ShowWindow(hwnd, SW_SHOW);
  SetForegroundWindow(hwnd);

  MSG msg;
  while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(hwnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  DeleteObject(st.font);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  if (st.accepted) settings = st.settings;
  return st.accepted;
}

}  // namespace

bool RunLauncher(rex::PathConfig& paths) {
  std::filesystem::path file = paths.config_path.parent_path() / "pgr3_launcher.toml";
  Settings s = Load(file);
  if (!paths.game_data_root.empty()) s.game_dir = paths.game_data_root.wstring();
  if (s.game_dir.empty()) {
    // First start: look next to the exe, then in the development tree.
    auto exe_dir = paths.config_path.parent_path();
    for (auto candidate : {exe_dir / "game", exe_dir / "assets",
                           exe_dir / ".." / ".." / ".." / "titles" / "pgr3" / "game"}) {
      std::error_code ec;
      auto dir = std::filesystem::weakly_canonical(candidate, ec);
      if (!ec && HasGame(dir.wstring())) {
        s.game_dir = dir.wstring();
        break;
      }
    }
  }

  const char* autopress = std::getenv("PGR3_AUTOPRESS");
  bool unattended = autopress && *autopress;
  bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  bool show = !unattended && ((s.show && REXCVAR_GET(show_launcher)) || shift ||
                              !HasGame(s.game_dir));
  if (show) {
    if (!RunDialog(s)) return false;
    Save(file, s);
  }
  if (std::filesystem::exists(file)) rex::cvar::LoadConfig(file);
  if (!s.game_dir.empty()) paths.game_data_root = s.game_dir;
  return true;
}

}  // namespace pgr3
