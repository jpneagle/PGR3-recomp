#pragma once

namespace pgr3 {

// Writes a report of the first fatal host exception to `path`.
void InstallCrashLog(const char* path);

}  // namespace pgr3
