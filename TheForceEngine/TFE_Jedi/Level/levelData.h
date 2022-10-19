#pragma once
//////////////////////////////////////////////////////////////////////
// Level
// Classic Dark Forces (DOS) Jedi derived Level structures and shared
// functionality used by systems such as collision detection,
// rendering, and INF.
//
// Copyright note:
// While the project as a whole is licensed under GPL-2.0, some of the
// code under TFE_Jedi/Level/ was derived from reverse-engineered
// code from "Dark Forces" (DOS) which is copyrighted by LucasArts.
//
// I consider the reverse-engineering to be "Fair Use" - a means of 
// supporting the games on other platforms and to improve support on
// existing platforms without claiming ownership of the games
// themselves or their IPs.
//
// That said using this code in a commercial project is risky without
// permission of the original copyright holders (LucasArts).
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include "rsector.h"

struct Task;

namespace TFE_Jedi
{
	struct AmbientSound
	{
		SoundSourceId soundId;
		SoundEffectId instanceId;
		vec3_fixed pos;
	};

	// Serializable State.
	struct LevelState
	{
		// Level Data.
		s32 minLayer = 0;
		s32 maxLayer = 0;
		s32 secretCount = 0;
		u32 sectorCount = 0;
		fixed16_16 parallax0 = 0;
		fixed16_16 parallax1 = 0;
		RSector* sectors = nullptr;

		// Gameplay sector pointers.
		RSector* bossSector = nullptr;
		RSector* mohcSector = nullptr;
		RSector* controlSector = nullptr;
		RSector* completeSector = nullptr;
				
		// Special level objects.
		Allocator* soundEmitters = nullptr;
		Allocator* safeLoc = nullptr;
		Allocator* ambientSounds = nullptr;
	};
	
	// State that should be reset.
	struct LevelInternalState
	{
		s32 podCount = 0;
		s32 spriteCount = 0;
		s32 fmeCount = 0;
		s32 soundCount = 0;
		s32 objectCount = 0;
		s32 textureCount = 0;

		TextureData** textures = nullptr;
		JediModel** pods = nullptr;
		JediWax**   sprites = nullptr;
		JediFrame** frames = nullptr;
		SoundSourceId* soundIds = nullptr;

		Task* ambientSoundTask = nullptr;
		Task* soundEmitterTask = nullptr;
	};

	// Serializable level state.
	extern LevelState s_levelState;
	// Internal level state.
	extern LevelInternalState s_levelIntState;
}
