#include <cstddef>
#include <iostream>

// macOS: sysctlbyname for cache-line size
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

// Linux: reading from sysfs
#if defined(__linux__)
#include <fstream>
#endif

// Windows: GetLogicalProcessorInformation
#if defined(_MSC_VER)
#include <vector>
#include <windows.h>
#endif

// -----------------------------
// 1) Compile-time fallback
// -----------------------------
constexpr size_t compile_cacheline =
  #if defined(__has_builtin) && __has_builtin(__builtin_hardware_destructive_interference_size)
    __builtin_hardware_destructive_interference_size();
  #else
    64;
  #endif

// -----------------------------
// 2) x86/x86_64 CPUID-based detection
// -----------------------------
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
size_t detect_x86() {
  unsigned eax, ebx, ecx, edx;
  __cpuid(1, eax, ebx, ecx, edx);
  return ((ebx >> 8) & 0xFF) * 8;
}
#else
constexpr size_t detect_x86() {
  return 0;
}
#endif

// -----------------------------
// 3) Linux sysfs detection
// -----------------------------
#if defined(__linux__)
size_t detect_linux_sysfs() {
  std::ifstream f("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
  size_t        v = 0;
  if (f >> v) return v; // bytes
  return 0;
}
#endif

// -----------------------------
// 4) Combined runtime detection
// -----------------------------
size_t get_cacheline_runtime() {
  size_t cls = 0;

  // macOS (Intel & Apple Silicon)
#if defined(__APPLE__)
  size_t len = sizeof(cls);
  if (sysctlbyname("hw.cachelinesize", &cls, &len, nullptr, 0) == 0) return cls;
#endif

    // Linux fallback: sysfs
#if defined(__linux__)
  cls = detect_linux_sysfs();
  if (cls > 0) return cls;
#endif

    // Windows: WinAPI Cache info
#if defined(_MSC_VER)
  DWORD needed = 0;
  GetLogicalProcessorInformation(nullptr, &needed);
  std::vector<char> buf(needed);
  if (GetLogicalProcessorInformation(
          reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION *>(buf.data()), &needed)) {
    auto  *info  = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION *>(buf.data());
    size_t count = needed / sizeof(*info);
    for (size_t i = 0; i < count; ++i) {
      if (info[i].Relationship == RelationCache && info[i].Cache.Level == 1)
        return info[i].Cache.LineSize;
    }
  }
#endif

  // x86 CPUID fallback
  if (size_t x = detect_x86()) return x;

  // final fallback: compile-time guess
  return compile_cacheline;
}

int main() {
  std::cout << get_cacheline_runtime();
  return 0;
}
