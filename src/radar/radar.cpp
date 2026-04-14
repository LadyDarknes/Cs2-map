#include "radar.h"
#include "../updater/updater.h"
#include "../utils/logger.h"
#include <chrono>
#include <set>
#include <thread>
#include <vector>

using json = nlohmann::json;

namespace radar {
json m_data;

std::string ReadString(const Memory &mem, uintptr_t address,
                       size_t maxLen = 128) {
  if (!address || maxLen == 0)
    return "";

  char buf[128] = {0};
  size_t readSize = (maxLen > 128) ? 128 : maxLen;

  struct DataBlock {
    char data[128];
  };

  DataBlock block = mem.read<DataBlock>(address);
  block.data[readSize - 1] = '\0';
  return std::string(block.data);
}

uintptr_t GetController(const Memory &mem, uintptr_t entityList, int32_t idx) {
  if (!entityList)
    return 0;

  uintptr_t listEntry = mem.read<uintptr_t>(entityList + 0x10);
  if (!listEntry)
    return 0;

  return mem.read<uintptr_t>(listEntry + idx * 0x70);
}

uintptr_t GetPawnFromHandle(const Memory &mem, uintptr_t entityList,
                            uint32_t handle) {
  if (!handle || handle == 0xFFFFFFFF)
    return 0;

  int32_t idx = handle & 0x7FFF;

  uintptr_t listEntry =
      mem.read<uintptr_t>(entityList + 0x8 * ((handle & 0x7FFF) >> 9) + 0x10);
  if (!listEntry)
    return 0;

  return mem.read<uintptr_t>(listEntry + 0x70 * (idx & 0x1FF));
}

uintptr_t GetEntityFromHandle(const Memory &mem, uintptr_t entityList,
                              uint32_t handle) {
  return GetPawnFromHandle(mem, entityList, handle);
}

void GetPlayers(const Memory &mem, uintptr_t clientBase) {
  m_data["m_players"] = json::array();

  uintptr_t entityList =
      mem.read<uintptr_t>(clientBase + updater::offsets.dwEntityList);
  if (!entityList)
    return;

  for (int i = 1; i < 64; ++i) {
    uintptr_t controller = GetController(mem, entityList, i);
    if (!controller)
      continue;

    uint32_t pawnHandle =
        mem.read<uint32_t>(controller + updater::offsets.m_hPlayerPawn);
    if (!pawnHandle || pawnHandle == 0xFFFFFFFF)
      continue;

    uintptr_t pawn = GetPawnFromHandle(mem, entityList, pawnHandle);
    if (!pawn)
      continue;

    uint32_t pawnHealth = mem.read<uint32_t>(pawn + updater::offsets.m_iHealth);
    if (pawnHealth == 0)
      continue;

    uint32_t team = mem.read<uint32_t>(pawn + updater::offsets.m_iTeamNum);

    uintptr_t namePtr = mem.read<uintptr_t>(
        controller + updater::offsets.m_sSanitizedPlayerName);
    if (!namePtr)
      continue;

    std::string name = ReadString(mem, namePtr, 64);
    if (name.empty() || name == "Unknown")
      continue;

    float x = mem.read<float>(pawn + updater::offsets.m_vOldOrigin);
    float y = mem.read<float>(pawn + updater::offsets.m_vOldOrigin + 4);

    float eyeYaw = mem.read<float>(pawn + updater::offsets.m_angEyeAngles + 4);

    json pData;
    pData["m_idx"] = i;
    pData["m_name"] = name;
    pData["m_team"] = team;
    pData["m_health"] = pawnHealth;
    pData["m_is_dead"] = (pawnHealth <= 0);
    pData["m_position"]["x"] = x;
    pData["m_position"]["y"] = y;
    pData["m_eye_angle"] = eyeYaw;
    pData["m_weapons"] = json::object();
    pData["m_weapons"]["m_melee"] = json::array();
    pData["m_weapons"]["m_utilities"] = json::array();

    m_data["m_players"].push_back(pData);
  }
}

void Run(const Memory &mem, uintptr_t clientBase) {
  if (!mem.IsConnected())
    return;

  m_data = json::object();

  uintptr_t globalVars =
      mem.read<uintptr_t>(clientBase + updater::offsets.dwGlobalVars);

  std::string mapName = "invalid";
  if (globalVars) {
    uintptr_t mapNamePtr = mem.read<uintptr_t>(globalVars + 0x188);
    if (mapNamePtr) {
      mapName = ReadString(mem, mapNamePtr, 64);
    }
  }

  if (mapName.empty() || mapName == "<empty>")
    mapName = "invalid";

  m_data["m_map"] = mapName;

  m_data["m_timestamp"] = GetTickCount64();

  uintptr_t localController = mem.read<uintptr_t>(
      clientBase + updater::offsets.dwLocalPlayerController);

  uint8_t localTeam = 0;
  if (localController) {
    localTeam =
        mem.read<uint8_t>(localController + updater::offsets.m_iTeamNum);
  }
  m_data["m_local_team"] = localTeam;

  GetPlayers(mem, clientBase);
}
}
