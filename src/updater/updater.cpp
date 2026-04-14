#include "updater.h"

#include "../utils/logger.h"

#include <Windows.h>

#include <filesystem>

#include <fstream>

#include <iostream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace updater {

GameOffsets offsets;

static std::string ReadLocalFile(const std::wstring &path) {

  std::ifstream file(path, std::ios::binary);

  if (!file.is_open()) {

    return "";
  }

  std::string content((std::istreambuf_iterator<char>(file)),

                      std::istreambuf_iterator<char>());

  return content;
}

static std::wstring GetExeDirectory() {

  wchar_t path[MAX_PATH];

  GetModuleFileNameW(NULL, path, MAX_PATH);

  std::wstring ws(path);

  size_t pos = ws.find_last_of(L"\\/");

  return (pos != std::wstring::npos) ? ws.substr(0, pos) : ws;
}

static std::wstring FindOffsetsDirectory() {
  std::wstring exeDir = GetExeDirectory();
  std::vector<std::wstring> searchPaths = {
      exeDir + L"\\offsets",
      exeDir + L"\\..\\offsets",
      exeDir + L"\\..\\..\\offsets",
      exeDir + L"\\..\\..\\..\\offsets",
      exeDir + L"\\..\\..\\src\\offsets",
      L"offsets",
      L"src\\offsets",
      L"..\\offsets",
      L"..\\src\\offsets",
      L"..\\..\\src\\offsets",
  };

  for (const auto &dir : searchPaths) {
    std::wstring testFile = dir + L"\\offsets.json";
    if (GetFileAttributesW(testFile.c_str()) != INVALID_FILE_ATTRIBUTES) {
      Log::Info("Found offsets directory: " + std::string(dir.begin(), dir.end()));
      return dir;
    }
  }

  return L"";
}

static ptrdiff_t GetClassField(const json &clientClasses,
                               const std::string &className,
                               const std::string &fieldName) {
  if (!clientClasses.contains(className)) {
    Log::Warning("Class not found: " + className);
    return 0;
  }

  const auto &cls = clientClasses[className];

  if (!cls.contains("fields") || !cls["fields"].contains(fieldName)) {
    Log::Warning("Field not found: " + className + "." + fieldName);
    return 0;
  }

  const auto &field = cls["fields"][fieldName];

  if (field.is_number()) {
    ptrdiff_t value = field.get<ptrdiff_t>();
    Log::Info("JSON[" + className + "." + fieldName + "] = " + std::to_string(value) + " (direct)");
    return value;
  } else if (field.is_object() && field.contains("offset")) {
    ptrdiff_t value = field["offset"].get<ptrdiff_t>();
    Log::Info("JSON[" + className + "." + fieldName + "] = " + std::to_string(value) + " (nested)");
    return value;
  } else {
    Log::Error("Invalid field format for: " + className + "." + fieldName);
    return 0;
  }
}

bool InitializeOffsets() {
  Log::Info("Loading offsets from local files...");
  std::wstring offsetsDir = FindOffsetsDirectory();
  if (offsetsDir.empty()) {
    Log::Error("Failed to find offsets directory. Make sure the 'offsets' "
               "folder with offsets.json and client_dll.json exists.");
    return false;
  }

  std::string offsetsRaw = ReadLocalFile(offsetsDir + L"\\offsets.json");
  if (offsetsRaw.empty()) {
    Log::Error("Failed to read offsets.json");
    return false;
  }

  try {
    auto offsetsJson = json::parse(offsetsRaw);
    auto clientOffsets = offsetsJson["client.dll"];
    offsets.dwLocalPlayerPawn = clientOffsets["dwLocalPlayerPawn"].get<ptrdiff_t>();
    offsets.dwEntityList = clientOffsets["dwEntityList"].get<ptrdiff_t>();
    offsets.dwViewMatrix = clientOffsets["dwViewMatrix"].get<ptrdiff_t>();
    offsets.dwGameEntitySystem = clientOffsets["dwGameEntitySystem"].get<ptrdiff_t>();
    offsets.dwGlobalVars = clientOffsets["dwGlobalVars"].get<ptrdiff_t>();
    offsets.dwLocalPlayerController = clientOffsets["dwLocalPlayerController"].get<ptrdiff_t>();
  } catch (const json::exception &e) {
    Log::Error("offsets.json parse error: " + std::string(e.what()));
    return false;
  }

  std::string clientDllRaw = ReadLocalFile(offsetsDir + L"\\client_dll.json");
  if (clientDllRaw.empty()) {
    Log::Error("Failed to read client_dll.json");
    return false;
  }

  try {
    auto clientDllJson = json::parse(clientDllRaw);
    const auto &classes = clientDllJson["client.dll"]["classes"];
    offsets.m_hPlayerPawn = GetClassField(classes, "CCSPlayerController", "m_hPlayerPawn");
    offsets.m_sSanitizedPlayerName = GetClassField(classes, "CCSPlayerController", "m_sSanitizedPlayerName");
    offsets.m_pInGameMoneyServices = GetClassField(classes, "CCSPlayerController", "m_pInGameMoneyServices");
    offsets.m_iHealth = GetClassField(classes, "C_BaseEntity", "m_iHealth");
    offsets.m_iTeamNum = GetClassField(classes, "C_BaseEntity", "m_iTeamNum");
    offsets.m_pGameSceneNode = GetClassField(classes, "C_BaseEntity", "m_pGameSceneNode");
    offsets.m_vOldOrigin = GetClassField(classes, "C_BasePlayerPawn", "m_vOldOrigin");
    offsets.m_pWeaponServices = GetClassField(classes, "C_BasePlayerPawn", "m_pWeaponServices");
    offsets.m_pItemServices = GetClassField(classes, "C_BasePlayerPawn", "m_pItemServices");
    offsets.m_vecAbsOrigin = GetClassField(classes, "CGameSceneNode", "m_vecAbsOrigin");
    offsets.m_angEyeAngles = GetClassField(classes, "C_CSPlayerPawn", "m_angEyeAngles");
    offsets.m_ArmorValue = GetClassField(classes, "C_CSPlayerPawn", "m_ArmorValue");
    offsets.m_flFlashBangTime = GetClassField(classes, "C_CSPlayerPawnBase", "m_flFlashBangTime");
    offsets.m_iAccount = GetClassField(classes, "CCSPlayerController_InGameMoneyServices", "m_iAccount");
    offsets.m_hActiveWeapon = GetClassField(classes, "CPlayer_WeaponServices", "m_hActiveWeapon");
    offsets.m_hMyWeapons = GetClassField(classes, "CPlayer_WeaponServices", "m_hMyWeapons");
    offsets.m_bHasHelmet = GetClassField(classes, "CCSPlayer_ItemServices", "m_bHasHelmet");
    offsets.m_bHasDefuser = GetClassField(classes, "CCSPlayer_ItemServices", "m_bHasDefuser");
    offsets.m_szName = GetClassField(classes, "CCSWeaponBaseVData", "m_szName");
    offsets.m_WeaponType = GetClassField(classes, "CCSWeaponBaseVData", "m_WeaponType");
    offsets.m_WeaponData = 0x08;
  } catch (const json::exception &e) {
    Log::Error("client_dll.json parse error: " + std::string(e.what()));
    return false;
  }

  Log::Fine("=== Loaded Offsets ===");
  Log::Info("dwEntityList=0x" + std::to_string(offsets.dwEntityList));
  Log::Info("dwLocalPlayerPawn=0x" + std::to_string(offsets.dwLocalPlayerPawn));
  Log::Info("dwLocalPlayerController=0x" + std::to_string(offsets.dwLocalPlayerController));
  Log::Info("dwLocalPlayerController=0x" +

            std::to_string(offsets.dwLocalPlayerController));

  Log::Info("m_hPlayerPawn=0x" + std::to_string(offsets.m_hPlayerPawn));

  Log::Info("m_iHealth=0x" + std::to_string(offsets.m_iHealth));

  Log::Info("m_iTeamNum=0x" + std::to_string(offsets.m_iTeamNum));

  Log::Info("m_pGameSceneNode=0x" + std::to_string(offsets.m_pGameSceneNode));

  Log::Info("m_vecAbsOrigin=0x" + std::to_string(offsets.m_vecAbsOrigin));

  Log::Info("m_angEyeAngles=0x" + std::to_string(offsets.m_angEyeAngles));

  Log::Info("m_sSanitizedPlayerName=0x" +

            std::to_string(offsets.m_sSanitizedPlayerName));

  return true;
}

}
