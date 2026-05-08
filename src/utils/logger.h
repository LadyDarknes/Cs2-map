#pragma once
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace Log {
inline HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
inline const std::string LogFile = "Logs.txt";
inline bool debugMode = false;
inline std::mutex logMutex;

inline void SetDebug(bool enabled) { debugMode = enabled; }

inline bool WriteLog(std::string ctx) {
  std::ofstream file(LogFile, std::ios::app);
  if (!file.is_open())
    return false;

  auto now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm timeinfo;
  localtime_s(&timeinfo, &now);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", &timeinfo);

  file << buffer << ctx << std::endl;
  file.close();
  return true;
}

inline void SplashScreen() {
  SetConsoleTextAttribute(hConsole, 11); // cyan
  std::cout << R"(
  ____ ____ ____    __        __   _                      _            
 / ___/ ___|___ \   \ \      / /__| |__  _ __ __ _  __| | __ _ _ __ 
| |   \___ \ __) |   \ \ /\ / / _ \ '_ \| '__/ _` |/ _` |/ _` | '__|
| |___ ___) / __/     \ V  V /  __/ |_) | | | (_| | (_| | (_| | |   
 \____|____/_____|     \_/\_/ \___|_.__/|_|  \__,_|\__,_|\__,_|_|   
                                                                    )"
            << std::endl;

  SetConsoleTextAttribute(hConsole, 13); // magenta
  std::cout << "  > Developed by swansizz" << std::endl;
  std::cout << "  > Status: Production Ready" << std::endl;
  std::cout << "  --------------------------------------------------------"
            << std::endl;
  SetConsoleTextAttribute(hConsole, 7); // white
}

inline void Info(std::string ctx) {
  std::lock_guard<std::mutex> lock(logMutex);
  SetConsoleTextAttribute(hConsole, 11);
  std::cout << "[INFO] ";
  SetConsoleTextAttribute(hConsole, 7);
  std::cout << ctx << std::endl;
}

inline void Warning(std::string ctx, bool pause = false) {
  std::lock_guard<std::mutex> lock(logMutex);
  SetConsoleTextAttribute(hConsole, 14);
  std::cout << "[WARN] ";
  SetConsoleTextAttribute(hConsole, 7);
  std::cout << ctx << std::endl;
  if (pause) {
    system("pause");
  }
}

inline void Error(std::string ctx, bool fatal = true) {
  std::lock_guard<std::mutex> lock(logMutex);
  SetConsoleTextAttribute(hConsole, 12);
  std::cout << "[FAIL] ";
  SetConsoleTextAttribute(hConsole, 7);
  std::cout << ctx << std::endl;
  if (fatal) {
    std::cout << "\n[!] Potential solution: Ensure cs2.exe is running and you "
                 "have Administrator privileges."
              << std::endl;
    system("pause");
    exit(-1);
  }
}

inline void Fine(std::string ctx) {
  std::lock_guard<std::mutex> lock(logMutex);
  SetConsoleTextAttribute(hConsole, 10);
  std::cout << "[SUCCESS] ";
  SetConsoleTextAttribute(hConsole, 7);
  std::cout << ctx << std::endl;
}

inline void Debug(std::string ctx) {
  if (!debugMode)
    return;
  std::lock_guard<std::mutex> lock(logMutex);
  SetConsoleTextAttribute(hConsole, 8);
  std::cout << "[DEBUG] " << ctx << std::endl;
  SetConsoleTextAttribute(hConsole, 7);
}

inline void Step(std::string ctx, std::function<bool()> task) {
  std::cout << "  > " << ctx << "... ";
  const char spinner[] = {'|', '/', '-', '\\'};
  int i = 0;

  // This is simple for synchronous tasks that are very fast,
  // but for longer ones we'd want a thread.
  // For now lets keep it simple or use a basic "done" indicator if it's too
  // fast
  if (task()) {
    SetConsoleTextAttribute(hConsole, 10);
    std::cout << "DONE" << std::endl;
  } else {
    SetConsoleTextAttribute(hConsole, 12);
    std::cout << "FAILED" << std::endl;
    exit(-1);
  }
  SetConsoleTextAttribute(hConsole, 7);
}

inline void Spinner(std::string ctx, std::atomic<bool> &done) {
  const char chars[] = {'|', '/', '-', '\\'};
  int i = 0;
  while (!done) {
    std::cout << "\r  > " << ctx << " " << chars[i++ % 4] << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::cout << "\r  > " << ctx << " [ " << "OK" << " ]   " << std::endl;
}

inline void PreviousLine() {
  std::lock_guard<std::mutex> lock(logMutex);
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
    COORD coord = csbi.dwCursorPosition;
    if (coord.Y > 0) {
      coord.Y--;
      coord.X = 0;
      SetConsoleCursorPosition(hConsole, coord);
    }
  }
}

inline void ClearConsole() {
  std::lock_guard<std::mutex> lock(logMutex);
  COORD topLeft = {0, 0};
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  DWORD written;
  if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
    DWORD consoleSize = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, ' ', consoleSize, topLeft, &written);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, consoleSize, topLeft,
                               &written);
    SetConsoleCursorPosition(hConsole, topLeft);
  }
}

} // namespace Log