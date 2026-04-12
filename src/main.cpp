#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "mapper/intel_driver.h"
#include "mapper/kdmapper.h"
#include "utils/cfg.h"
#include "utils/logger.h"
#include "utils/nt.h"
#include "utils/utils.h"

bool IsDriverRunning(const LPCWSTR name);
bool CheckArg(const int argc, wchar_t **argv, const wchar_t *arg);

int wmain(const int argc, wchar_t **argv) {
  bool free = false;
  bool indPagesMode = CheckArg(argc, argv, L"securemode");
  bool legacyImg = CheckArg(argc, argv, L"legacyimg");
  bool copyHeader = false;
  bool passAllocationPtr = false;

  Log::Info("Driver name " + cfg::name + " made by " + cfg::author);

  if (IsDriverRunning(L"\\\\.\\NsiCoreSys")) {
    Log::Error("Kernel mode driver is already mapped");
    return -1;
  }

  BYTE *img = nullptr;
  if (!legacyImg) {
    if (cfg::image.empty()) {
      std::wstring driver_path =
          kdmUtils::GetCurrentAppFolder() + L"\\driver.sys";
      if (!kdmUtils::ReadFileToMemory(driver_path.c_str(), &cfg::image)) {
        Log::Error("Failed to find driver.sys next to the executable!");
        return -1;
      }
    }
    RollingVectorProcedure(cfg::image, cfg::key);
    img = cfg::image.data();
  } else {
    if (cfg::imageLegacy.empty()) {
      Log::Error("Driver image is empty");
      return -1;
    }
    RollingVectorProcedure(cfg::imageLegacy, cfg::key);
    img = cfg::imageLegacy.data();
  }

  if (!NT_SUCCESS(intel_driver::Load())) {
    Log::Error("Failed to connect to intel driver");
    return -1;
  }

  kdmapper::AllocationMode mode = kdmapper::AllocationMode::AllocatePool;
  if (indPagesMode)
    mode = kdmapper::AllocationMode::AllocateIndependentPages;

  NTSTATUS exitCode = 0;
  if (!kdmapper::MapDriver(img, 0, 0, free, !copyHeader, mode,
                           passAllocationPtr, nullptr, &exitCode)) {
    intel_driver::Unload();
    Log::Error("Failed to map driver");
    return -1;
  }

  if (!NT_SUCCESS(intel_driver::Unload()))
    Log::Warning("Warning: failed to unload intel driver", true);

  Log::Fine("Driver mapped successfully");
  system("pause");
  return 0;
}

bool CheckArg(const int argc, wchar_t **argv, const wchar_t *arg) {
  size_t plen = wcslen(arg);
  for (int i = 1; i < argc; i++) {
    if (wcslen(argv[i]) == plen + 1ull && _wcsicmp(&argv[i][1], arg) == 0 &&
        argv[i][0] == '/')
      return true;
    else if (wcslen(argv[i]) == plen + 2ull &&
             _wcsicmp(&argv[i][2], arg) == 0 && argv[i][0] == '-' &&
             argv[i][1] == '-')
      return true;
  }
  return false;
}

bool IsDriverRunning(const LPCWSTR name) {
  HANDLE kernelDriver =
      CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, nullptr);
  if (kernelDriver == INVALID_HANDLE_VALUE)
    return false;

  CloseHandle(kernelDriver);
  return true;
}