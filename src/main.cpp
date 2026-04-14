#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <shellapi.h>
#include <string>
#include <thread>
#include <vector>

#include "nlohmann/json.hpp"

#include "mapper/intel_driver.h"
#include "mapper/kdmapper.h"
#include "utils/cfg.h"
#include "utils/logger.h"
#include "utils/nt.h"
#include "utils/utils.h"

#include "driver/ext/memory.h"
#include "easywsclient/easywsclient.hpp"
#include "radar/radar.h"
#include "updater/updater.h"

bool IsDriverRunning(const LPCWSTR name);
bool CheckArg(const int argc, wchar_t **argv, const wchar_t *arg);

int wmain(const int argc, wchar_t **argv) {
  Log::SetDebug(CheckArg(argc, argv, L"debug"));
  Log::SplashScreen();
  bool free = false;
  bool indPagesMode = CheckArg(argc, argv, L"securemode");
  bool legacyImg = CheckArg(argc, argv, L"legacyimg");
  bool copyHeader = false;
  bool passAllocationPtr = false;

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

  Log::Info("Connecting to Intel Driver...");
  if (!NT_SUCCESS(intel_driver::Load())) {
    Log::Error("Failed to connect to Intel Driver components.");
    return -1;
  }

  kdmapper::AllocationMode mode = kdmapper::AllocationMode::AllocatePool;
  if (indPagesMode)
    mode = kdmapper::AllocationMode::AllocateIndependentPages;

  NTSTATUS exitCode = 0;
  Log::Info("Mapping kernel driver...");
  if (!kdmapper::MapDriver(img, 0, 0, free, !copyHeader, mode,
                           passAllocationPtr, nullptr, &exitCode)) {
    intel_driver::Unload();
    Log::Error("Kernel mapping verification failed.");
    return -1;
  }

  if (!NT_SUCCESS(intel_driver::Unload()))
    Log::Warning("Warning: failed to unload intel driver", true);

  Log::Fine("Kernel environment established.");

  Log::Info("Synchronizing game offsets...");
  if (!updater::InitializeOffsets()) {
    Log::Error("Failed to synchronize dynamic offsets. Check your internet "
               "connection.");
    return -1;
  }

  Memory mem(L"cs2.exe");
  if (!mem.IsConnected()) {
    Log::Error("Failed to find cs2.exe. Please run the game first.");
    system("pause");
    return -1;
  }

  uintptr_t clientBase = mem.GetModuleBase(L"client.dll");
  if (!clientBase) {
    Log::Error("Failed to get client.dll base.");
    system("pause");
    return -1;
  }

  Log::Fine("Game data synchronized.");
  Log::Fine("CS2 session linked successfully.");

  // Clear console and show clean final state
  Log::ClearConsole();
  Log::SplashScreen();

  SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10); // green
  std::cout << "  > Status: Injected Successfully" << std::endl;
  SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7); // white
  std::cout << "  --------------------------------------------------------"
            << std::endl;

  std::wstring current_path = kdmUtils::GetCurrentAppFolder();
  std::wstring app_js_path = current_path + L"\\webapp\\ws\\app.js";

  if (GetFileAttributesW(app_js_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    app_js_path = current_path + L"\\..\\..\\webapp\\ws\\app.js";
    if (GetFileAttributesW(app_js_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
      app_js_path = current_path + L"\\..\\webapp\\ws\\app.js";
    }
  }

  Log::Info("Starting Node.js backend...");
  std::wstring node_cmd =
      L"start /B cmd /c \"cd /d " + current_path +
      L"\\..\\..\\webapp && npm run dev\" > node_log.txt 2>&1";
  _wsystem(node_cmd.c_str());

  std::atomic<bool> connected(false);
  std::thread spinnerThread(
      [&]() { Log::Spinner("Connecting to local UI", std::ref(connected)); });

  Sleep(3000);

  ShellExecuteA(NULL, "open", "http://localhost:5173", NULL, NULL,
                SW_SHOWNORMAL);

  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);

  static auto ws =
      easywsclient::WebSocket::from_url("ws://127.0.0.1:22006/cs2_webradar");
  connected = true;
  spinnerThread.join();

  if (!ws) {
    Log::Error("Communication link failed. Ensure Node.js and all dependencies "
               "are installed.");
    return -1;
  }

  Log::Fine("CS2 Web Radar is now Active.");
  Log::Info("Keep this window open while playing.");

  int loopCount = 0;
  auto lastDebugTime = std::chrono::high_resolution_clock::now();
  auto lastSendTime = std::chrono::high_resolution_clock::now();

  while (true) {
    auto loopStart = std::chrono::high_resolution_clock::now();

    radar::Run(mem, clientBase);

    auto afterRadar = std::chrono::high_resolution_clock::now();
    auto radarDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                             afterRadar - loopStart)
                             .count();

    if (loopCount % 50 == 0) {
      auto now = std::chrono::high_resolution_clock::now();
      auto elapsedSinceLastDebug =
          std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                lastDebugTime)
              .count();
      auto elapsedSinceLastSend =
          std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                lastSendTime)
              .count();

      Log::Debug("Loop=" + std::to_string(loopCount) +
                 " | RadarRead=" + std::to_string(radarDuration) + "ms" +
                 " | TimeSinceLastSend=" +
                 std::to_string(elapsedSinceLastSend) + "ms" + " | Players=" +
                 std::to_string(radar::m_data["m_players"].size()) +
                 " | Map=" + radar::m_data["m_map"].get<std::string>());
      lastDebugTime = now;
    }
    loopCount++;

    if (ws) {
      auto sendStart = std::chrono::high_resolution_clock::now();

      std::string data = radar::m_data.dump(
          -1, ' ', false, nlohmann::json::error_handler_t::replace);
      size_t dataSize = data.size();

      if (!data.empty() && data != "{}") {
        ws->send(data);
        lastSendTime = std::chrono::high_resolution_clock::now();

        auto sendDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(lastSendTime -
                                                                  sendStart)
                .count();
        if (loopCount % 50 == 0) {
          Log::Debug("WebSocket SEND | Bytes=" + std::to_string(dataSize) +
                     " | Serialize+Send=" + std::to_string(sendDuration) +
                     "us");
        }
      }
      ws->poll();
    } else {
      Log::Warning("WebSocket is null, attempting reconnect...");
      ws = easywsclient::WebSocket::from_url(
          "ws://127.0.0.1:22006/cs2_webradar");
    }
    Sleep(16); // ~60 FPS
  }

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