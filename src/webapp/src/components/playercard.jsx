import { useState, useEffect } from "react";
import MaskedIcon from "./maskedicon";
import { teamEnum } from "../utilities/utilities";

const PlayerCard = ({ playerData, isOnRightSide, localTeam }) => {
  const [modelName, setModelName] = useState(playerData.m_model_name);
  const [characterSrc, setCharacterSrc] = useState("");
  const isLocalPlayer = playerData.m_is_local === true;
  const isTeammate = playerData.m_team === localTeam;
  const accentColor = isLocalPlayer
    ? "#ffffff"
    : isTeammate
      ? "#2dd4bf"
      : "#ef4444";

  const fallbackCharacter =
    playerData.m_team === teamEnum.counterTerrorist
      ? "ctm_fbi"
      : "tm_phoenix";

  useEffect(() => {
    if (playerData.m_model_name)
      setModelName(playerData.m_model_name);
  }, [playerData.m_model_name]);

  useEffect(() => {
    const resolvedModel = playerData.m_model_name || modelName || fallbackCharacter;
    setCharacterSrc(`./assets/characters/${resolvedModel}.png`);
  }, [fallbackCharacter, modelName, playerData.m_model_name]);

  return (
    <li
      style={{ opacity: `${(playerData.m_is_dead && `0.5`) || `1`}` }}
      className={`flex ${isOnRightSide && `flex-row-reverse`}`}
    >
      <div
        className={`flex flex-col gap-[0.375rem] justify-center items-center`}
      >
        <div
          className={`hover:cursor-pointer`}
          style={{ color: accentColor, fontWeight: isLocalPlayer ? 700 : 400 }}
          onClick={() =>
            window.open(
              `https://steamcommunity.com/profiles/${playerData.m_steam_id}`,
              "_blank",
              "noopener,noreferrer"
            )
          }
        >
          {playerData.m_name}
        </div>
        <div
          className={`w-0 h-0 border-solid border-t-[12px] border-r-[8px] border-b-[12px] border-l-[8px]`}
          style={{
            borderColor: `${accentColor} transparent transparent transparent`,
          }}
        ></div>
        <img
          className={`h-[8rem] ${isOnRightSide && `scale-x-[-1]`}`}
          src={characterSrc}
          onError={(event) => {
            const fallbackSrc = `./assets/characters/${fallbackCharacter}.png`;
            if (!event.currentTarget.src.endsWith(`${fallbackCharacter}.png`)) {
              event.currentTarget.src = fallbackSrc;
              setCharacterSrc(fallbackSrc);
            } else {
              event.currentTarget.style.display = "none";
            }
          }}
        />
      </div>

      <div
        className={`flex flex-col ${
          isOnRightSide && `flex-row-reverse`
        } justify-center gap-2`}
      >
        <span
          className={`${isOnRightSide && `flex justify-end`} text-radar-green`}
        >
          ${playerData.m_money}
        </span>

        <div className={`flex ${isOnRightSide && `flex-row-reverse`} gap-2`}>
          <div className="flex gap-[4px] items-center">
            <MaskedIcon
              path={`./assets/icons/health.svg`}
              height={16}
              color={`bg-radar-secondary`}
            />
            <span className="text-radar-primary">{playerData.m_health}</span>
          </div>

          <div className="flex gap-[4px] items-center">
            <MaskedIcon
              path={`./assets/icons/${
                (playerData.m_has_helmet && `kevlar_helmet`) || `kevlar`
              }.svg`}
              height={16}
              color={`bg-radar-secondary`}
            />
            <span className="text-radar-primary">{playerData.m_armor}</span>
          </div>
        </div>

        <div className={`flex ${isOnRightSide && `flex-row-reverse`} gap-3`}>
          {playerData.m_weapons && playerData.m_weapons.m_primary && (
            <MaskedIcon
              path={`./assets/icons/${playerData.m_weapons.m_primary}.svg`}
              height={28}
              color={`${
                (playerData.m_weapons.m_active ==
                  playerData.m_weapons.m_primary &&
                  `bg-radar-primary`) ||
                `bg-radar-secondary`
              }`}
            />
          )}

          {playerData.m_weapons && playerData.m_weapons.m_secondary && (
            <MaskedIcon
              path={`./assets/icons/${playerData.m_weapons.m_secondary}.svg`}
              height={28}
              color={`${
                (playerData.m_weapons.m_active ==
                  playerData.m_weapons.m_secondary &&
                  `bg-radar-primary`) ||
                `bg-radar-secondary`
              }`}
            />
          )}

          {playerData.m_weapons &&
            playerData.m_weapons.m_melee &&
            playerData.m_weapons.m_melee.map((melee) => (
              <MaskedIcon
                key={melee}
                path={`./assets/icons/${melee}.svg`}
                height={28}
                color={`${
                  (playerData.m_weapons.m_active == melee &&
                    `bg-radar-primary`) ||
                  `bg-radar-secondary`
                }`}
              />
            ))}

          {(!playerData.m_weapons ||
            (!playerData.m_weapons.m_primary &&
              !playerData.m_weapons.m_secondary &&
              (!playerData.m_weapons.m_melee ||
                playerData.m_weapons.m_melee.length === 0))) && (
            <span className="text-xs text-slate-400">weapon data yok</span>
          )}
        </div>

        <div className={`flex flex-col relative`}>
          <div
            className={`flex ${
              isOnRightSide && `flex-row-reverse`
            } gap-9 mt-3 items-center`}
          >
            {playerData.m_weapons &&
              playerData.m_weapons.m_utilities &&
              playerData.m_weapons.m_utilities.map((utility) => (
                <MaskedIcon
                  key={utility}
                  path={`./assets/icons/${utility}.svg`}
                  height={28}
                  color={`${
                    (playerData.m_weapons.m_active == utility &&
                      `bg-radar-primary`) ||
                    `bg-radar-secondary`
                  }`}
                />
              ))}

            {[
              ...Array(
                Math.max(
                  4 -
                    ((playerData.m_weapons &&
                      playerData.m_weapons.m_utilities &&
                      playerData.m_weapons.m_utilities.length) ||
                      0),
                  0
                )
              ),
            ].map((_, i) => (
              <div
                key={i}
                className="rounded-full w-[6px] h-[6px] bg-radar-primary"
              ></div>
            ))}

            {(playerData.m_team == teamEnum.counterTerrorist &&
              playerData.m_has_defuser && (
                <MaskedIcon
                  path={`./assets/icons/defuser.svg`}
                  height={28}
                  color={`bg-radar-secondary`}
                />
              )) ||
              (playerData.m_team == teamEnum.terrorist &&
                playerData.m_has_bomb && (
                  <MaskedIcon
                    path={`./assets/icons/c4.svg`}
                    height={28}
                    color={
                      ((playerData.m_weapons &&
                        playerData.m_weapons.m_active) == `c4` &&
                        `bg-radar-primary`) ||
                      `bg-radar-secondary`
                    }
                  />
                ))}
          </div>
        </div>
      </div>
    </li>
  );
};

export default PlayerCard;
