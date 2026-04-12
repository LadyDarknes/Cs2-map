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
  std::string res;
  char buf[128];
  for (size_t i = 0; i < maxLen; ++i) {
    char c = mem.read<char>(address + i);
    if (!c)
      break;
    res += c;
  }
  return res;
}

uintptr_t GetEntity(const Memory &mem, uintptr_t clientBase, int32_t idx) {
  uintptr_t entityList =
      mem.read<uintptr_t>(clientBase + updater::offsets.dwEntityList);
  if (!entityList)
    return 0;

  uintptr_t entryList = mem.read<uintptr_t>(entityList + (8 * (idx >> 9) + 16));
  if (!entryList)
    return 0;

  return mem.read<uintptr_t>(entryList + (120 * (idx & 0x1FF)));
}

void GetPlayers(const Memory &mem, uintptr_t clientBase) {
  m_data["m_players"] = json::array();

  for (int i = 1; i <= 64; ++i) {
    uintptr_t controller = GetEntity(mem, clientBase, i);
    if (!controller)
      continue;

    // Basic Controller info
    uint32_t health =
        mem.read<uint32_t>(controller + updater::offsets.m_iHealth);
    uint32_t team =
        mem.read<uint32_t>(controller + updater::offsets.m_iTeamNum);

    // Check if it's a valid player controller (sanity check on name)
    uintptr_t namePtr = mem.read<uintptr_t>(
        controller + updater::offsets.m_sSanitizedPlayerName);
    std::string name = "Unknown";
    if (namePtr) {
      name = ReadString(mem, namePtr, 64);
    }

    if (name == "Unknown" || name.empty())
      continue;

    uintptr_t pawnHandle =
        mem.read<uintptr_t>(controller + updater::offsets.m_hPlayerPawn);
    if (!pawnHandle)
      continue;

    // Find Pawn
    uintptr_t entityList =
        mem.read<uintptr_t>(clientBase + updater::offsets.dwEntityList);
    uintptr_t entryList = mem.read<uintptr_t>(
        entityList + (8 * ((pawnHandle & 0x7FFF) >> 9) + 16));
    if (!entryList)
      continue;
    uintptr_t pawn =
        mem.read<uintptr_t>(entryList + (120 * (pawnHandle & 0x1FF)));
    if (!pawn)
      continue;

    // Position & State
    uint32_t pawnHealth = mem.read<uint32_t>(pawn + updater::offsets.m_iHealth);
    bool isDead = (pawnHealth <= 0);

    uintptr_t sceneNode =
        mem.read<uintptr_t>(pawn + updater::offsets.m_pGameSceneNode);
    float x = mem.read<float>(sceneNode + updater::offsets.m_vecAbsOrigin);
    float y = mem.read<float>(sceneNode + updater::offsets.m_vecAbsOrigin + 4);
    float eyeYaw = mem.read<float>(pawn + updater::offsets.m_angEyeAngles + 4);

    json pData;
    pData["m_idx"] = i;
    pData["m_name"] = name;
    pData["m_team"] = team;
    pData["m_health"] = pawnHealth;
    pData["m_is_dead"] = isDead;
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

  // Read Map Name
  uintptr_t globalVars =
      mem.read<uintptr_t>(clientBase + updater::offsets.dwGlobalVars);
  uintptr_t mapNamePtr =
      mem.read<uintptr_t>(globalVars + 0x188); // Current map name offset
  std::string mapName = ReadString(mem, mapNamePtr, 64);
  if (mapName.empty() || mapName == "<empty>")
    mapName = "invalid";

  m_data["m_map"] = mapName;

  // Read Local Team
  uintptr_t localController = mem.read<uintptr_t>(
      clientBase + updater::offsets.dwLocalPlayerController);
  uint32_t localTeam =
      mem.read<uint32_t>(localController + updater::offsets.m_iTeamNum);
  m_data["m_local_team"] = localTeam;

  GetPlayers(mem, clientBase);
}
} // namespace radar
