// Logs fatal host exceptions (code, faulting address, module offsets of return addresses on the
// stack) before the process dies, so crashes can be mapped with pgr3.map.

#include "crash_log.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <windows.h>

#ifdef REX_DEBUG_FP_TRAP
// Read by the recompiled code whenever it rewrites MXCSR (rex/ppc/context.h).
extern "C" volatile int rex_debug_fp_trap = 0;
#endif

namespace pgr3 {
namespace {

FILE* g_out = nullptr;

void ModuleOf(uintptr_t addr, char* name, size_t name_size, uintptr_t* base) {
  HMODULE mod = nullptr;
  name[0] = 0;
  *base = 0;
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(addr), &mod)) {
    char path[MAX_PATH];
    GetModuleFileNameA(mod, path, MAX_PATH);
    const char* slash = strrchr(path, '\\');
    snprintf(name, name_size, "%s", slash ? slash + 1 : path);
    *base = reinterpret_cast<uintptr_t>(mod);
  }
}

void PrintAddr(const char* what, uintptr_t addr) {
  char name[MAX_PATH];
  uintptr_t base;
  ModuleOf(addr, name, sizeof(name), &base);
  if (base)
    fprintf(g_out, "  %s %016llx  %s+%llx\n", what, (unsigned long long)addr, name,
            (unsigned long long)(addr - base));
  else
    fprintf(g_out, "  %s %016llx\n", what, (unsigned long long)addr);
}

bool IsFatal(DWORD code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_PRIV_INSTRUCTION:
    case STATUS_HEAP_CORRUPTION:
    case STATUS_STACK_BUFFER_OVERRUN:
      return true;
  }
  // Floating point exceptions (0xC000008D..0xC0000093, 0xC00002B4/5).
  return (code >= 0xC000008D && code <= 0xC0000093) || code == 0xC00002B4 || code == 0xC00002B5;
}

#ifdef REX_DEBUG_FP_TRAP
// PGR3_FP_TRAP_MS=<ms>: from that time on, log each distinct instruction that raises the SSE
// invalid-operation exception (i.e. first produces a NaN), then let it continue masked.
constexpr int kMaxTrapSites = 2048;
uintptr_t g_trap_sites[kMaxTrapSites];
volatile LONG g_trap_site_count = 0;
ULONGLONG g_trap_start_tick = 0;

thread_local bool t_fp_stepping = false;

bool HandleFpTrap(EXCEPTION_POINTERS* ep) {
  CONTEXT* c = ep->ContextRecord;
  if (!rex_debug_fp_trap || !(c->MxCsr & 0x01)) return false;  // IE flag
  uintptr_t rip = c->Rip;
  LONG n = g_trap_site_count;
  bool known = false;
  for (LONG i = 0; i < n && i < kMaxTrapSites; ++i) known |= g_trap_sites[i] == rip;
  if (!known && n < kMaxTrapSites) {
    g_trap_sites[InterlockedIncrement(&g_trap_site_count) - 1] = rip;
    fprintf(g_out, "fp invalid at %llu ms thread %lu", GetTickCount64() - g_trap_start_tick,
            GetCurrentThreadId());
    PrintAddr("", rip);
    fflush(g_out);
  }
  // Run this one instruction masked, then unmask again from the single-step trap.
  c->MxCsr = (c->MxCsr | 0x80) & ~0x3Fu;
  c->EFlags |= 0x100;
  t_fp_stepping = true;
  return true;
}
#endif

LONG CALLBACK Handler(EXCEPTION_POINTERS* ep) {
  DWORD code = ep->ExceptionRecord->ExceptionCode;
#ifdef REX_DEBUG_FP_TRAP
  if ((code == 0xC00002B5 || code == 0xC00002B4 || code == 0xC0000090) && HandleFpTrap(ep))
    return EXCEPTION_CONTINUE_EXECUTION;
  if (code == EXCEPTION_SINGLE_STEP && t_fp_stepping) {
    t_fp_stepping = false;
    ep->ContextRecord->MxCsr = (ep->ContextRecord->MxCsr & ~0x3Fu) & ~0x80u;
    return EXCEPTION_CONTINUE_EXECUTION;
  }
#endif
  if (!IsFatal(code)) return EXCEPTION_CONTINUE_SEARCH;
  static volatile LONG once = 0;
  if (InterlockedExchange(&once, 1)) return EXCEPTION_CONTINUE_SEARCH;
  CONTEXT* c = ep->ContextRecord;
  fprintf(g_out, "=== host exception %08lx on thread %lu ===\n", code, GetCurrentThreadId());
  PrintAddr("rip", c->Rip);
  if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
    fprintf(g_out, "  %s address %016llx\n",
            ep->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
            (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1]);
  fprintf(g_out, "  mxcsr %08lx  x87cw %04x\n", c->MxCsr, c->FltSave.ControlWord);
  fprintf(g_out, "  rax %016llx rbx %016llx rcx %016llx rdx %016llx\n", c->Rax, c->Rbx, c->Rcx,
          c->Rdx);
  fprintf(g_out, "  rsi %016llx rdi %016llx r8  %016llx r9  %016llx\n", c->Rsi, c->Rdi, c->R8,
          c->R9);
  // Return addresses on the stack that point into a loaded module.
  auto* sp = reinterpret_cast<uintptr_t*>(c->Rsp);
  int shown = 0;
  for (int i = 0; i < 4096 && shown < 32; ++i) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(sp + i, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) break;
    uintptr_t v = sp[i];
    char name[MAX_PATH];
    uintptr_t base;
    ModuleOf(v, name, sizeof(name), &base);
    if (base && v - base > 0x1000) {
      PrintAddr("stack", v);
      ++shown;
    }
  }
  fflush(g_out);
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

void InstallCrashLog(const char* path) {
  g_out = fopen(path, "w");
  if (!g_out) g_out = stderr;
  AddVectoredExceptionHandler(1, Handler);
#ifdef REX_DEBUG_FP_TRAP
  if (const char* ms = getenv("PGR3_FP_TRAP_MS")) {
    g_trap_start_tick = GetTickCount64();
    DWORD delay = strtoul(ms, nullptr, 10);
    CreateThread(nullptr, 0, [](void* p) -> DWORD {
      Sleep(DWORD(uintptr_t(p)));
      rex_debug_fp_trap = 1;
      return 0;
    }, reinterpret_cast<void*>(uintptr_t(delay)), 0, nullptr);
  }
#endif
}

}  // namespace pgr3
