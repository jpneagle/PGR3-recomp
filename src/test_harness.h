#pragma once

namespace rex {
class Runtime;
struct RuntimeConfig;
}  // namespace rex

namespace pgr3 {

// Swaps the input backend for a scripted pad when PGR3_AUTOPRESS is set.
void ConfigureTestInput(rex::RuntimeConfig& config);
// Saves the guest output periodically when PGR3_DUMP_FRAMES is set.
void StartFrameDumper(rex::Runtime* runtime);
void StopTestHarness();

}  // namespace pgr3
