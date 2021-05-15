#pragma once
//////////////////////////////////////////////////////////////////////
// Collision System
// Classic Dark Forces (DOS) Jedi derived Collision system. This is
// the core collision system used by Dark Forces.
//
// Copyright note:
// While the project as a whole is licensed under GPL-2.0, some of the
// code under TFE_Collision/ was derived from reverse-engineered
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
#include <TFE_Level/level.h>
#include <TFE_Level/core_math.h>

struct RSector;
struct SecObject;
struct RWall;

struct CollisionInterval
{
	fixed16_16 x0;
	fixed16_16 x1;
	fixed16_16 y0;
	fixed16_16 y1;
	fixed16_16 z0;
	fixed16_16 z1;
	fixed16_16 move;
	fixed16_16 dirX;
	fixed16_16 dirZ;
};

namespace TFE_Collision
{
	void collision_getHitPoint(fixed16_16* x, fixed16_16* z);
	RSector* collision_tryMove(RSector* sector, fixed16_16 x0, fixed16_16 z0, fixed16_16 x1, fixed16_16 z1);
	RSector* collision_moveObj(SecObject* obj, fixed16_16 dx, fixed16_16 dz);
	RWall* collision_pathWallCollision(RSector* sector);

	SecObject* collision_getObjectCollision(RSector* sector, CollisionInterval* interval, SecObject* prevObj);
}
