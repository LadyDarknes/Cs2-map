#include "updater.h"
#include "../utils/logger.h"
#include <Windows.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <wininet.h>

using json = nlohmann::json;

namespace updater {
GameOffsets offsets;

std::string FetchURL(const std::string &url) {
  std::string result = "";
  HINTERNET hInternet = InternetOpenA("cs2-webradar by swansizz",
                                      INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
  if (!hInternet)
    return result;

  HINTERNET hUrl =
      InternetOpenUrlA(hInternet, url.c_str(), NULL, 0,
                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
  if (hUrl) {
    char buffer[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) &&
           bytesRead > 0) {
      result.append(buffer, bytesRead);
    }
    InternetCloseHandle(hUrl);
  }
  InternetCloseHandle(hInternet);
  return result;
}

bool InitializeOffsets() {
  Log::Info("Fetching dynamic offsets from GitHub...");

  std::string offsetsJsonRaw = FetchURL("https://raw.githubusercontent.com/a2x/"
                                        "cs2-dumper/main/output/offsets.json");
  std::string clientJsonRaw =
      FetchURL("https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/"
               "client_dll.json");

  if (offsetsJsonRaw.empty() || clientJsonRaw.empty()) {
    Log::Error("Failed to fetch offsets from internet.");
    return false;
  }

  try {
    auto offsetsJson = json::parse(offsetsJsonRaw);
    auto clientJson = json::parse(clientJsonRaw);

    // parse offsets
    auto clientOffsets = offsetsJson["client.dll"];
    offsets.dwLocalPlayerPawn =
        clientOffsets["dwLocalPlayerPawn"].get<ptrdiff_t>();
    offsets.dwEntityList = clientOffsets["dwEntityList"].get<ptrdiff_t>();
    offsets.dwViewMatrix = clientOffsets["dwViewMatrix"].get<ptrdiff_t>();
    offsets.dwGameEntitySystem =
        clientOffsets["dwGameEntitySystem"].get<ptrdiff_t>();
    offsets.dwGlobalVars = clientOffsets["dwGlobalVars"].get<ptrdiff_t>();
    offsets.dwLocalPlayerController =
        clientOffsets["dwLocalPlayerController"].get<ptrdiff_t>();

    // parse client schema (classes)
    auto clientSchema = clientJson["client.dll"]["classes"];
    offsets.m_hPlayerPawn =
        clientSchema["CCSPlayerController"]["fields"]["m_hPlayerPawn"]
            .get<ptrdiff_t>();
    offsets.m_iHealth =
        clientSchema["C_BaseEntity"]["fields"]["m_iHealth"].get<ptrdiff_t>();
    offsets.m_iTeamNum =
        clientSchema["C_BaseEntity"]["fields"]["m_iTeamNum"].get<ptrdiff_t>();
    offsets.m_vOldOrigin =
        clientSchema["C_BasePlayerPawn"]["fields"]["m_vOldOrigin"]
            .get<ptrdiff_t>();
    offsets.m_pGameSceneNode =
        clientSchema["C_BaseEntity"]["fields"]["m_pGameSceneNode"]
            .get<ptrdiff_t>();
    offsets.m_vecAbsOrigin =
        clientSchema["CGameSceneNode"]["fields"]["m_vecAbsOrigin"]
            .get<ptrdiff_t>();
    offsets.m_angEyeAngles =
        clientSchema["C_CSPlayerPawn"]["fields"]["m_angEyeAngles"]
            .get<ptrdiff_t>();
    offsets.m_flFlashBangTime =
        clientSchema["C_CSPlayerPawnBase"]["fields"]["m_flFlashBangTime"]
            .get<ptrdiff_t>();

    // New additional fields
    offsets.m_sSanitizedPlayerName =
        clientSchema["CCSPlayerController"]["fields"]["m_sSanitizedPlayerName"]
            .get<ptrdiff_t>();
    offsets.m_pInGameMoneyServices =
        clientSchema["CCSPlayerController"]["fields"]["m_pInGameMoneyServices"]
            .get<ptrdiff_t>();
    offsets.m_iAccount = clientSchema["CCSPlayerController_InGameMoneyServices"]
                                     ["fields"]["m_iAccount"]
                                         .get<ptrdiff_t>();
    offsets.m_pWeaponServices =
        clientSchema["C_BasePlayerPawn"]["fields"]["m_pWeaponServices"]
            .get<ptrdiff_t>();
    offsets.m_hActiveWeapon =
        clientSchema["CPlayer_WeaponServices"]["fields"]["m_hActiveWeapon"]
            .get<ptrdiff_t>();
    offsets.m_hMyWeapons =
        clientSchema["CPlayer_WeaponServices"]["fields"]["m_hMyWeapons"]
            .get<ptrdiff_t>();
    offsets.m_ArmorValue =
        clientSchema["C_CSPlayerPawn"]["fields"]["m_ArmorValue"]
            .get<ptrdiff_t>();
    offsets.m_pItemServices =
        clientSchema["C_BasePlayerPawn"]["fields"]["m_pItemServices"]
            .get<ptrdiff_t>();
    offsets.m_bHasHelmet =
        clientSchema["CCSPlayer_ItemServices"]["fields"]["m_bHasHelmet"]
            .get<ptrdiff_t>();
    offsets.m_bHasDefuser =
        clientSchema["CCSPlayer_ItemServices"]["fields"]["m_bHasDefuser"]
            .get<ptrdiff_t>();

    // Weapon data offsets (subclass based)
    offsets.m_WeaponData = 0x08; // Fixed usually
    offsets.m_szName = clientSchema["CCSWeaponBaseVData"]["fields"]["m_szName"]
                           .get<ptrdiff_t>();
    offsets.m_WeaponType =
        clientSchema["CCSWeaponBaseVData"]["fields"]["m_WeaponType"]
            .get<ptrdiff_t>();

    Log::Fine("Offsets updated successfully.");
    return true;
  } catch (const json::exception &e) {
    Log::Error("JSON parse error: " + std::string(e.what()));
    return false;
  }
}
} // namespace updater
