// pgr3 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

#include <cstdlib>

#include "crash_log.h"
#include "launcher.h"
#include "test_harness.h"

class Pgr3App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Pgr3App>(new Pgr3App(ctx, "pgr3",
        PPCImageConfig));
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (!pgr3::RunLauncher(paths)) std::exit(0);
  }

  void OnPostInitLogging() override { pgr3::InstallCrashLog("pgr3_crash.txt"); }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    // PGR3 needs the Xenos GPU emulation; default to it unless --gpu_plugin was given.
    if (config.gpu_plugin.empty()) config.gpu_plugin = "xenos";
    pgr3::ConfigureTestInput(config);
  }

  void OnPostSetup() override { pgr3::StartFrameDumper(runtime()); }
  void OnShutdown() override { pgr3::StopTestHarness(); }

  // Override virtual hooks for customization:
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
};
