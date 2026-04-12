#pragma once
#include <string>

namespace updater {
struct GameOffsets {
  // client.dll offsets
  ptrdiff_t dwLocalPlayerPawn;
  ptrdiff_t dwEntityList;
  ptrdiff_t dwViewMatrix;
  ptrdiff_t dwGameEntitySystem;
  ptrdiff_t dwGlobalVars;
  ptrdiff_t dwLocalPlayerController;

  // client.dll netvars/schema
  ptrdiff_t m_hPlayerPawn;
  ptrdiff_t m_iHealth;
  ptrdiff_t m_iTeamNum;
  ptrdiff_t m_vOldOrigin;
  ptrdiff_t m_pGameSceneNode;
  ptrdiff_t m_vecAbsOrigin;
  ptrdiff_t m_angEyeAngles;
  ptrdiff_t m_flFlashBangTime;

  // Additional fields
  ptrdiff_t m_sSanitizedPlayerName;
  ptrdiff_t m_pInGameMoneyServices;
  ptrdiff_t m_iAccount;
  ptrdiff_t m_pWeaponServices;
  ptrdiff_t m_hActiveWeapon;
  ptrdiff_t m_WeaponData;
  ptrdiff_t m_szName;
  ptrdiff_t m_WeaponType;
  ptrdiff_t m_ArmorValue;
  ptrdiff_t m_pItemServices;
  ptrdiff_t m_bHasHelmet;
  ptrdiff_t m_bHasDefuser;
  ptrdiff_t m_hMyWeapons;
};

extern GameOffsets offsets;

bool InitializeOffsets();
} // namespace updater
