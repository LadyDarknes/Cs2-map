#include "radar.h"
#include "../updater/updater.h"
#include "../utils/logger.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace radar {
json m_data;

namespace {
constexpr int kMaxPlayers = 64;
constexpr size_t kEntityStride = 0x70;
constexpr size_t kEntityPageEntries = 512;
constexpr size_t kEntityPageSize = kEntityStride * kEntityPageEntries;
constexpr size_t kLegacyDriverMaxRead = 0x1000;

std::unordered_map<uintptr_t, std::string> g_nameCache;
std::unordered_map<uintptr_t, std::string> g_stringCache;

template <typename T>
T ReadFromBuffer(const std::vector<unsigned char> &buffer, size_t offset) {
  T value{};
  if (offset + sizeof(T) > buffer.size())
    return value;

  std::memcpy(&value, buffer.data() + offset, sizeof(T));
  return value;
}

std::vector<unsigned char> ReadBlock(const Memory &mem, uintptr_t address,
                                     size_t size) {
  if (!address || size == 0)
    return {};

  std::vector<unsigned char> buffer(size);
  size_t offset = 0;
  while (offset < size) {
    size_t chunk = size - offset;
    if (chunk > kLegacyDriverMaxRead) {
      chunk = kLegacyDriverMaxRead;
    }
    mem.read_raw(address + offset, buffer.data() + offset, chunk);
    offset += chunk;
  }
  return buffer;
}
} // namespace

std::string ReadString(const Memory &mem, uintptr_t address,
                       size_t maxLen = 128) {
  if (!address || maxLen == 0)
    return "";

  struct StringBuffer {
    char data[128];
  };

  StringBuffer buf = mem.read<StringBuffer>(address);
  buf.data[127] = '\0';

  std::string safeStr;
  safeStr.reserve(maxLen);
  for (size_t i = 0; i < maxLen && i < sizeof(buf.data) && buf.data[i] != '\0';
       i++) {
    unsigned char c = static_cast<unsigned char>(buf.data[i]);
    if (c >= 32 && c <= 126) {
      safeStr += static_cast<char>(c);
    }
  }

  if (safeStr.empty())
    return "Bilinmeyen";
  return safeStr;
}

void GetPlayers(const Memory &mem, uintptr_t clientBase) {
  auto startTotal = std::chrono::high_resolution_clock::now();
  m_data["m_players"] = json::array();

  auto startEntityList = std::chrono::high_resolution_clock::now();
  uintptr_t entityList =
      mem.read<uintptr_t>(clientBase + updater::offsets.dwEntityList);
  auto endEntityList = std::chrono::high_resolution_clock::now();
  auto entityListTime = std::chrono::duration_cast<std::chrono::microseconds>(
                            endEntityList - startEntityList)
                            .count();

  Log::Debug("entityList=0, time=" + std::to_string(entityListTime) + "us");

  uintptr_t controllerPagePtr = mem.read<uintptr_t>(entityList + 0x10);
  if (!controllerPagePtr) {
    Log::Debug("listEntry=0");
    return;
  }

  auto controllerPage = ReadBlock(mem, controllerPagePtr, kEntityPageSize);
  if (controllerPage.size() != kEntityPageSize) {
    Log::Debug("controllerPage read failed");
    return;
  }

  int validPlayers = 0;
  long long totalGetController = 0;
  long long totalGetPawn = 0;
  long long totalReadData = 0;
  long long totalReadString = 0;
  std::unordered_map<uint32_t, std::vector<unsigned char>> pawnPages;

  for (int i = 1; i < kMaxPlayers; ++i) {
    auto startController = std::chrono::high_resolution_clock::now();
    uintptr_t controller = ReadFromBuffer<uintptr_t>(
        controllerPage, static_cast<size_t>(i) * kEntityStride);
    auto endController = std::chrono::high_resolution_clock::now();
    totalGetController += std::chrono::duration_cast<std::chrono::microseconds>(
                              endController - startController)
                              .count();

    if (!controller)
      continue;

    ptrdiff_t controllerMinOffset =
        (updater::offsets.m_sSanitizedPlayerName <
         updater::offsets.m_hPlayerPawn)
            ? updater::offsets.m_sSanitizedPlayerName
            : updater::offsets.m_hPlayerPawn;
    ptrdiff_t controllerMaxOffset =
        ((updater::offsets.m_sSanitizedPlayerName +
          static_cast<ptrdiff_t>(sizeof(uintptr_t))) >
         (updater::offsets.m_hPlayerPawn +
          static_cast<ptrdiff_t>(sizeof(uint32_t))))
            ? (updater::offsets.m_sSanitizedPlayerName +
               static_cast<ptrdiff_t>(sizeof(uintptr_t)))
            : (updater::offsets.m_hPlayerPawn +
               static_cast<ptrdiff_t>(sizeof(uint32_t)));
    auto controllerBlock = ReadBlock(
        mem, controller + controllerMinOffset,
        static_cast<size_t>(controllerMaxOffset - controllerMinOffset));

    uint32_t pawnHandle = ReadFromBuffer<uint32_t>(
        controllerBlock, static_cast<size_t>(updater::offsets.m_hPlayerPawn -
                                             controllerMinOffset));
    if (!pawnHandle || pawnHandle == 0xFFFFFFFF)
      continue;

    auto startPawn = std::chrono::high_resolution_clock::now();
    uint32_t pawnIndex = pawnHandle & 0x7FFF;
    uint32_t pawnPageIndex = pawnIndex >> 9;
    uintptr_t pawnPagePtr =
        mem.read<uintptr_t>(entityList + 0x10 + (0x8 * pawnPageIndex));

    uintptr_t pawn = 0;
    if (pawnPagePtr) {
      auto &pawnPage = pawnPages[pawnPageIndex];
      if (pawnPage.empty()) {
        pawnPage = ReadBlock(mem, pawnPagePtr, kEntityPageSize);
      }
      pawn = ReadFromBuffer<uintptr_t>(
          pawnPage, static_cast<size_t>(pawnIndex & 0x1FF) * kEntityStride);
    }
    auto endPawn = std::chrono::high_resolution_clock::now();
    totalGetPawn += std::chrono::duration_cast<std::chrono::microseconds>(
                        endPawn - startPawn)
                        .count();

    if (!pawn)
      continue;

    auto startReadData = std::chrono::high_resolution_clock::now();
    struct PositionData {
      float x, y, z;
    };
    struct AngleData {
      float pitch, yaw, roll;
    };

    ptrdiff_t pawnStatsMinOffset =
        (updater::offsets.m_iHealth < updater::offsets.m_iTeamNum)
            ? updater::offsets.m_iHealth
            : updater::offsets.m_iTeamNum;
    ptrdiff_t pawnStatsMaxOffset =
        ((updater::offsets.m_iHealth +
          static_cast<ptrdiff_t>(sizeof(uint32_t))) >
         (updater::offsets.m_iTeamNum +
          static_cast<ptrdiff_t>(sizeof(uint32_t))))
            ? (updater::offsets.m_iHealth +
               static_cast<ptrdiff_t>(sizeof(uint32_t)))
            : (updater::offsets.m_iTeamNum +
               static_cast<ptrdiff_t>(sizeof(uint32_t)));

    auto pawnStatsBlock =
        ReadBlock(mem, pawn + pawnStatsMinOffset,
                  static_cast<size_t>(pawnStatsMaxOffset - pawnStatsMinOffset));
    auto posBlock = ReadBlock(mem, pawn + updater::offsets.m_vOldOrigin,
                              sizeof(PositionData));
    auto angleBlock = ReadBlock(mem, pawn + updater::offsets.m_angEyeAngles,
                                sizeof(AngleData));

    PositionData pos = ReadFromBuffer<PositionData>(posBlock, 0);
    AngleData angles = ReadFromBuffer<AngleData>(angleBlock, 0);
    uint32_t pawnHealth = ReadFromBuffer<uint32_t>(
        pawnStatsBlock,
        static_cast<size_t>(updater::offsets.m_iHealth - pawnStatsMinOffset));
    uint32_t team = ReadFromBuffer<uint32_t>(
        pawnStatsBlock,
        static_cast<size_t>(updater::offsets.m_iTeamNum - pawnStatsMinOffset));
    uintptr_t namePtr = ReadFromBuffer<uintptr_t>(
        controllerBlock,
        static_cast<size_t>(updater::offsets.m_sSanitizedPlayerName -
                            controllerMinOffset));
    auto endReadData = std::chrono::high_resolution_clock::now();
    totalReadData += std::chrono::duration_cast<std::chrono::microseconds>(
                         endReadData - startReadData)
                         .count();

    if (pawnHealth == 0 || !namePtr)
      continue;

    auto startString = std::chrono::high_resolution_clock::now();
    std::string name;
    auto cachedName = g_nameCache.find(controller);
    if (cachedName != g_nameCache.end()) {
      name = cachedName->second;
    } else {
      name = ReadString(mem, namePtr, 64);
      if (!name.empty() && name != "Unknown" && name != "Bilinmeyen") {
        g_nameCache[controller] = name;
      }
    }
    auto endString = std::chrono::high_resolution_clock::now();
    totalReadString += std::chrono::duration_cast<std::chrono::microseconds>(
                           endString - startString)
                           .count();

    if (name.empty() || name == "Unknown")
      continue;

    json pData;
    pData["m_idx"] = i;
    pData["m_name"] = name;
    pData["m_team"] = team;
    pData["m_health"] = pawnHealth;
    pData["m_is_dead"] = (pawnHealth <= 0);
    pData["m_position"]["x"] = pos.x;
    pData["m_position"]["y"] = pos.y;
    pData["m_eye_angle"] = angles.yaw;
    pData["m_weapons"] = json::object();
    pData["m_weapons"]["m_melee"] = json::array();
    pData["m_weapons"]["m_utilities"] = json::array();

    m_data["m_players"].push_back(pData);
    validPlayers++;
  }

  auto endTotal = std::chrono::high_resolution_clock::now();
  auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                       endTotal - startTotal)
                       .count();

  Log::Debug("Total=" + std::to_string(totalTime) + "ms | " +
             "Players=" + std::to_string(validPlayers) + " | " +
             "GetController=" + std::to_string(totalGetController / 1000) +
             "ms | " + "GetPawn=" + std::to_string(totalGetPawn / 1000) +
             "ms | " + "ReadData=" + std::to_string(totalReadData / 1000) +
             "ms | " + "ReadString=" + std::to_string(totalReadString / 1000) +
             "ms | " + "EntityList=" + std::to_string(entityListTime / 1000) +
             "ms");
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
      auto cachedMap = g_stringCache.find(mapNamePtr);
      if (cachedMap != g_stringCache.end()) {
        mapName = cachedMap->second;
      } else {
        mapName = ReadString(mem, mapNamePtr, 64);
        if (!mapName.empty())
          g_stringCache[mapNamePtr] = mapName;
      }
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
} // namespace radar
