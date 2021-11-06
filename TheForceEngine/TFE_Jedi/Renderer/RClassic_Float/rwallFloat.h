#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "../rlimits.h"
#include "../rwallRender.h"
#include "../rwallSegment.h"

struct RSector;
struct EdgePairFixed;

namespace TFE_Jedi
{
	namespace RClassic_Float
	{
		void wall_process(RWall* wall);
		s32  wall_mergeSort(RWallSegmentFloat* segOutList, s32 availSpace, s32 start, s32 count);

		void wall_drawSolid(RWallSegmentFloat* wallSegment);
		void wall_drawTransparent(RWallSegmentFloat* wallSegment, EdgePairFixed* edge);
		void wall_drawMask(RWallSegmentFloat* wallSegment);
		void wall_drawBottom(RWallSegmentFloat* wallSegment);
		void wall_drawTop(RWallSegmentFloat* wallSegment);
		void wall_drawTopAndBottom(RWallSegmentFloat* wallSegment);

		void wall_drawSkyTop(RSector* sector);
		void wall_drawSkyTopNoWall(RSector* sector);
		void wall_drawSkyBottom(RSector* sector);
		void wall_drawSkyBottomNoWall(RSector* sector);

		void wall_addAdjoinSegment(s32 length, s32 x0, fixed16_16 top_dydx, fixed16_16 y1, fixed16_16 bot_dydx, fixed16_16 y0, RWallSegmentFloat* wallSegment);

		void wall_setupAdjoinDrawFlags(RWall* wall);
		void wall_computeTexelHeights(RWall* wall);

		// Sprite code for now because so much is shared.
		void sprite_drawFrame(u8* basePtr, WaxFrame* frame, SecObject* obj);
	}
}