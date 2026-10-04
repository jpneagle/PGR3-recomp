#pragma once

#include <rex/rex_app.h>

namespace pgr3 {

// Shows the startup settings window (unless disabled) and applies the saved choices as cvars.
// Returns false when the user quits from the window.
bool RunLauncher(rex::PathConfig& paths);

}  // namespace pgr3
