#include "diagnostics.h"

#include "version.h"

#include <windows.h>

#include <dbghelp.h>
#include <shlobj.h>
#include <strsafe.h>

namespace {
constexpr size_t kPathChars = MAX_PATH + 64;
// Keeps a single runaway log from filling the disk; one previous file is kept.
constexpr DWORD kLogRotateBytes = 512 * 1024;

wchar_t g_diagnosticsDir[kPathChars] = L"";
wchar_t g_logPath[kPathChars] = L"";
wchar_t g_exePath[kPathChars] = L"";

// Guards against re-entering the handler when the report itself faults.
volatile LONG g_handling = 0;

typedef BOOL(WINAPI* MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
  const PMINIDUMP_EXCEPTION_INFORMATION, const PMINIDUMP_USER_STREAM_INFORMATION,
  const PMINIDUMP_CALLBACK_INFORMATION);

MiniDumpWriteDumpFn g_miniDumpWriteDump = nullptr;

bool EnsureDirectory(const wchar_t* dir) {
  if (dir == nullptr || dir[0] == L'\0') return false;
  if (CreateDirectoryW(dir, nullptr)) return true;
  // Already existing is the expected case after the first run.
  return GetLastError() == ERROR_ALREADY_EXISTS;
}

void ResolvePaths() {
  wchar_t modulePath[kPathChars] = L"";
  if (GetModuleFileNameW(nullptr, modulePath, static_cast<DWORD>(kPathChars)) != 0) {
    StringCchCopyW(g_exePath, kPathChars, modulePath);
  }

  PWSTR knownPath = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &knownPath))) {
    StringCchPrintfW(g_diagnosticsDir, kPathChars, L"%s\\MyBuddy", knownPath);
    CoTaskMemFree(knownPath);
    EnsureDirectory(g_diagnosticsDir);
  }

  if (g_diagnosticsDir[0] == L'\0') {
    wchar_t tempPath[kPathChars] = L"";
    if (GetTempPathW(static_cast<DWORD>(kPathChars), tempPath) != 0) {
      StringCchPrintfW(g_diagnosticsDir, kPathChars, L"%sMyBuddy", tempPath);
      EnsureDirectory(g_diagnosticsDir);
    }
  }

  if (g_diagnosticsDir[0] == L'\0') {
    StringCchCopyW(g_diagnosticsDir, kPathChars, L".");
  }
  StringCchPrintfW(g_logPath, kPathChars, L"%s\\diagnostics.log", g_diagnosticsDir);
}

void RotateLogIfHuge() {
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(g_logPath, GetFileExInfoStandard, &data)) return;
  const ULONGLONG size = (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  if (size <= kLogRotateBytes) return;

  wchar_t previous[kPathChars] = L"";
  StringCchPrintfW(previous, kPathChars, L"%s.previous", g_logPath);
  DeleteFileW(previous);
  MoveFileExW(g_logPath, previous, MOVEFILE_REPLACE_EXISTING);
}

// Appends one UTF-8 line. FILE_APPEND_DATA makes concurrent writes from two
// instances harmless, and a failure here must never affect the caller.
void WriteLine(const wchar_t* text) {
  if (g_logPath[0] == L'\0' || text == nullptr) return;

  HANDLE file = CreateFileW(g_logPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;

  // Large enough for the whole 4096-wchar report even in double-byte scripts.
  char utf8[16384];
  const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8,
    static_cast<int>(sizeof(utf8)), nullptr, nullptr);
  if (length > 0) {
    DWORD written = 0;
    WriteFile(file, utf8, static_cast<DWORD>(length - 1), &written, nullptr);
  }
  CloseHandle(file);
}

void AppendTimestamped(const wchar_t* text) {
  SYSTEMTIME local{};
  GetLocalTime(&local);
  wchar_t line[4096] = L"";
  StringCchPrintfW(line, 4096, L"[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n",
    local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond,
    text == nullptr ? L"" : text);
  WriteLine(line);
}

// Resolves the module that contains the faulting address so the report is
// readable without a symbol server.
void DescribeFaultingModule(DWORD64 address, wchar_t* out, size_t outChars) {
  out[0] = L'\0';
  MEMORY_BASIC_INFORMATION info{};
  if (VirtualQuery(reinterpret_cast<LPCVOID>(static_cast<UINT_PTR>(address)), &info, sizeof(info)) == 0 ||
      info.AllocationBase == nullptr) {
    StringCchPrintfW(out, outChars, L"unknown");
    return;
  }

  wchar_t modulePath[kPathChars] = L"";
  if (GetModuleFileNameW(static_cast<HMODULE>(info.AllocationBase), modulePath,
      static_cast<DWORD>(kPathChars)) == 0) {
    StringCchPrintfW(out, outChars, L"base=0x%08llX", static_cast<unsigned long long>(
      reinterpret_cast<UINT_PTR>(info.AllocationBase)));
    return;
  }

  const wchar_t* slash = modulePath;
  for (const wchar_t* p = modulePath; *p != L'\0'; ++p) {
    if (*p == L'\\' || *p == L'/') slash = p + 1;
  }
  const DWORD64 base = static_cast<DWORD64>(reinterpret_cast<UINT_PTR>(info.AllocationBase));
  StringCchPrintfW(out, outChars, L"%s+0x%08llX", slash,
    static_cast<unsigned long long>(address >= base ? address - base : 0));
}

bool WriteMiniDump(EXCEPTION_POINTERS* exceptionPointers, wchar_t* outPath, size_t outChars) {
  outPath[0] = L'\0';
  if (g_miniDumpWriteDump == nullptr || g_diagnosticsDir[0] == L'\0') return false;

  SYSTEMTIME local{};
  GetLocalTime(&local);
  StringCchPrintfW(outPath, outChars, L"%s\\crash-%04d%02d%02d-%02d%02d%02d.dmp",
    g_diagnosticsDir, local.wYear, local.wMonth, local.wDay,
    local.wHour, local.wMinute, local.wSecond);

  HANDLE file = CreateFileW(outPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    outPath[0] = L'\0';
    return false;
  }

  MINIDUMP_EXCEPTION_INFORMATION dumpInfo{};
  dumpInfo.ThreadId = GetCurrentThreadId();
  dumpInfo.ExceptionPointers = exceptionPointers;
  dumpInfo.ClientPointers = FALSE;

  const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
  const BOOL ok = g_miniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
    dumpType, &dumpInfo, nullptr, nullptr);
  CloseHandle(file);
  if (!ok) {
    DeleteFileW(outPath);
    outPath[0] = L'\0';
    return false;
  }
  return true;
}

// Explains the common codes that reach this handler. 0xC000041D is the one
// users hit for startup failures: the fault happened inside a window
// procedure, and the OS re-raises it as "fatal user callback exception".
const wchar_t* DescribeExceptionCode(DWORD code) {
  switch (code) {
    case 0xC0000005UL:
      return L"access violation";
    case 0xC0000094UL:
      return L"integer division by zero";
    case 0xC00000FDUL:
      return L"stack overflow";
    case 0xC0000409UL:
      return L"fail-fast (buffer overrun / corrupted state)";
    case 0xC0000417UL:
      return L"invalid CRT parameter";
    case 0xC000041DUL:
      return L"exception escaped a window-procedure callback";
    case 0xE06D7363UL:
      return L"unhandled C++ exception";
    case 0xC0000135UL:
      return L"required DLL was not found";
    case 0xC0000142UL:
      return L"DLL initialization failed";
    case 0xC000007BUL:
      return L"wrong architecture or mismatched dependency";
    default:
      return L"";
  }
}

void DescribeAccessViolation(const EXCEPTION_RECORD* record, wchar_t* out, size_t outChars) {
  out[0] = L'\0';
  if (record == nullptr || record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
      record->NumberParameters < 2) {
    return;
  }

  const wchar_t* kind = L"unknown";
  switch (record->ExceptionInformation[0]) {
    case 0:
      kind = L"read";
      break;
    case 1:
      kind = L"write";
      break;
    case 8:
      kind = L"execute";
      break;
    default:
      break;
  }
  StringCchPrintfW(out, outChars, L"%s at 0x%08llX", kind,
    static_cast<unsigned long long>(record->ExceptionInformation[1]));
}

LONG WINAPI HandleUnhandledException(EXCEPTION_POINTERS* exceptionPointers) {
  // A fault inside this handler would otherwise recurse forever.
  if (InterlockedExchange(&g_handling, 1) != 0) return EXCEPTION_EXECUTE_HANDLER;

  const EXCEPTION_RECORD* record =
    exceptionPointers != nullptr ? exceptionPointers->ExceptionRecord : nullptr;

  wchar_t moduleText[kPathChars] = L"";
  wchar_t accessText[128] = L"";
  wchar_t dumpPath[kPathChars] = L"";
  const DWORD64 faultAddress = record != nullptr
    ? static_cast<DWORD64>(reinterpret_cast<UINT_PTR>(record->ExceptionAddress))
    : 0;

  DescribeFaultingModule(faultAddress, moduleText, kPathChars);
  DescribeAccessViolation(record, accessText, 128);
  const bool dumped = WriteMiniDump(exceptionPointers, dumpPath, kPathChars);

  // Built separately so the report format below keeps a fixed argument count.
  wchar_t violationLine[256] = L"";
  if (accessText[0] != L'\0') {
    StringCchPrintfW(violationLine, 256, L"  access violation: %s\r\n", accessText);
  } else if (record != nullptr && record->ExceptionCode == 0xC000041DUL) {
    // The nested record of the original fault lives in the dump only; reading
    // it here could fault again while the process is already broken.
    StringCchCopyW(violationLine, 256,
      L"  callback exception: the original fault is recorded in the dump\r\n");
  }

  wchar_t report[4096] = L"";
  StringCchPrintfW(report, 4096,
    L"Unexpected error during startup or operation.\r\n"
    L"  version: %s\r\n"
    L"  exe: %s\r\n"
    L"  pid=%lu tid=%lu\r\n"
    L"  exception code: 0x%08lX (%s) flags=0x%08lX\r\n"
    L"  fault address: 0x%08llX (%s)\r\n"
    L"%s"
    L"  dump: %s\r\n"
    L"  log: %s\r\n",
    MYBUDDY_VERSION_STRING,
    g_exePath[0] != L'\0' ? g_exePath : L"(unknown)",
    GetCurrentProcessId(), GetCurrentThreadId(),
    record != nullptr ? static_cast<unsigned long>(record->ExceptionCode) : 0UL,
    record != nullptr ? DescribeExceptionCode(record->ExceptionCode) : L"",
    record != nullptr ? static_cast<unsigned long>(record->ExceptionFlags) : 0UL,
    static_cast<unsigned long long>(faultAddress),
    moduleText,
    violationLine,
    dumped ? dumpPath : L"(not written)",
    g_logPath[0] != L'\0' ? g_logPath : L"(not available)");

  WriteLine(L"\r\n");
  AppendTimestamped(report);
  // The MessageBox below is what turns a silent exit into an actionable report.
  wchar_t message[4096] = L"";
  StringCchPrintfW(message, 4096,
    L"MyBuddy (%s) stopped unexpectedly.\r\n\r\n%s\r\n\r\nSend these files when reporting the problem.",
    MYBUDDY_VERSION_STRING, report);
  MessageBoxW(nullptr, message, L"MyBuddy error", MB_OK | MB_ICONERROR | MB_TOPMOST);

  InterlockedExchange(&g_handling, 0);
  return EXCEPTION_EXECUTE_HANDLER;
}
}  // namespace

void InstallCrashHandler() {
  ResolvePaths();
  RotateLogIfHuge();

  // dbghelp is loaded at startup so the handler never needs to load a DLL
  // from a faulted process; a missing dump just falls back to the text log.
  if (HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll")) {
    g_miniDumpWriteDump = reinterpret_cast<MiniDumpWriteDumpFn>(
      GetProcAddress(dbghelp, "MiniDumpWriteDump"));
  }

  SetUnhandledExceptionFilter(HandleUnhandledException);
}

void LogDiagnostic(const wchar_t* message) {
  AppendTimestamped(message);
}

const wchar_t* GetDiagnosticsLogPath() {
  return g_logPath;
}
