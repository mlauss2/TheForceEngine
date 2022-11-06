#include <cstring>

#include "dragon.h"
#include "actorModule.h"
#include "../logic.h"
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/pickup.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_DarkForces/weapon.h>
#include <TFE_Game/igame.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	struct KellDragon
	{
		Logic logic;
		PhysicsActor actor;

		s32 animIndex;
		fixed16_16 painLevel;
		SoundEffectId painSndId;
		JBool retreat;
		Tick nextTick;
		s32 pad;
	};

	struct DragonMoveAnim
	{
		s32 framerate;
		fixed16_16 speed;
	};

	static const fixed16_16 s_kellDragon_NearDist = FIXED(25);
	static const fixed16_16 s_kellDragon_MedDist  = FIXED(45);
	static const fixed16_16 s_kellDragon_FarDist  = FIXED(60);
	
	static const DragonMoveAnim s_kellDragonAnim[4] =
	{
		{  5, FIXED(9)  },
		{  7, FIXED(12) },
		{  8, FIXED(16) },
		{ 10, FIXED(24) },
	};

	static SoundSourceId s_kellSound0 = NULL_SOUND;
	static SoundSourceId s_kellSound1 = NULL_SOUND;
	static SoundSourceId s_kellSound2 = NULL_SOUND;
	static SoundSourceId s_kellSound3 = NULL_SOUND;
	static SoundSourceId s_kellSound4 = NULL_SOUND;

	static KellDragon* s_curDragon = nullptr;
	static s32 s_dragonNum = 0;

	void kellDragon_handleDamage(MessageType msg)
	{
		struct LocalContext
		{
			KellDragon* dragon;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			LogicAnimation tmpAnim;
		};
		task_begin_ctx;

		local(dragon) = s_curDragon;
		local(obj) = local(dragon)->logic.obj;
		local(physicsActor) = &local(dragon)->actor;
		local(anim) = &local(physicsActor)->anim;

		if (local(physicsActor)->alive)
		{
			task_localBlockBegin;
				ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
				local(physicsActor)->hp -= proj->dmg;
				local(dragon)->painLevel = min(FIXED(100), local(dragon)->painLevel + FIXED(20));
			task_localBlockEnd;

			if (local(physicsActor)->hp <= 0)
			{
				local(physicsActor)->state = 5;
				msg = MSG_RUN_TASK;
			}
			else
			{
				sound_stop(local(dragon)->painSndId);
				local(dragon)->painSndId = sound_playCued(s_kellSound2, local(obj)->posWS);

				if (random(100) <= 20)
				{
					task_localBlockBegin;
						ActorTarget* target = &local(physicsActor)->moveMod.target;
						target->flags |= 8;
						memcpy(&local(tmpAnim), local(anim), sizeof(LogicAnimation) - 4);

						local(anim)->flags |= 1;
						actor_setupAnimation2(local(obj), 12, local(anim));
						local(anim)->frameRate = 8;
					task_localBlockEnd;

					do
					{
						entity_yield(TASK_NO_DELAY);
					} while (!(local(anim)->flags & 2) || msg != MSG_RUN_TASK);

					task_localBlockBegin;
						memcpy(local(anim), &local(tmpAnim), sizeof(LogicAnimation) - 4);
						actor_setupAnimation2(local(obj), local(anim)->animId, local(anim));

						ActorTarget* target = &local(physicsActor)->moveMod.target;
						target->flags &= 0xfffffff7;
					task_localBlockEnd;
				}
				msg = MSG_DAMAGE;
			}
		}
		task_setMessage(msg);
		task_end;
	}

	void kellDragon_handleExplosion(MessageType msg)
	{
		struct LocalContext
		{
			KellDragon* dragon;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			ActorTarget* target;
			LogicAnimation tmp;
			vec3_fixed vel;
		};
		task_begin_ctx;

		local(dragon) = s_curDragon;
		local(obj) = local(dragon)->logic.obj;
		local(physicsActor) = &local(dragon)->actor;
		local(anim) = &local(physicsActor)->anim;
		local(target) = &local(physicsActor)->moveMod.target;

		if (local(physicsActor)->alive)
		{
			task_localBlockBegin;
				fixed16_16 dmg   = s_msgArg1;
				fixed16_16 force = s_msgArg2;

				local(physicsActor)->hp -= dmg;
				local(dragon)->painLevel = min(FIXED(100), local(dragon)->painLevel + FIXED(40));
			
				vec3_fixed pushVel;
				vec3_fixed pos = { local(obj)->posWS.x, local(obj)->posWS.y - local(obj)->worldHeight, local(obj)->posWS.z };
				computeExplosionPushDir(&pos, &pushVel);
				local(vel) = { mul16(force, pushVel.x), mul16(force, pushVel.y), mul16(force, pushVel.z) };
			task_localBlockEnd;

			if (local(physicsActor)->hp <= 0)
			{
				local(physicsActor)->state = 5;
				local(physicsActor)->vel = local(vel);
				msg = MSG_RUN_TASK;
			}
			else
			{
				// Add one if the value is negative.
				local(vel).x += (local(vel).x < 0 ? 1 : 0);
				local(vel).y += (local(vel).y < 0 ? 1 : 0);
				local(vel).z += (local(vel).z < 0 ? 1 : 0);
				local(physicsActor)->vel.x = local(vel).x >> 1;
				local(physicsActor)->vel.y = local(vel).y >> 1;
				local(physicsActor)->vel.z = local(vel).z >> 1;

				sound_stop(local(dragon)->painSndId);
				local(dragon)->painSndId = sound_playCued(s_kellSound2, local(obj)->posWS);

				if (random(100) <= 20)
				{
					local(target)->flags |= 8;

					// Save the animation.
					memcpy(&local(tmp), local(anim), sizeof(LogicAnimation) - 4);

					// Set the animation to 12.
					local(anim)->flags |= 1;
					actor_setupAnimation2(local(obj), 12, local(anim));
					local(anim)->frameRate = 8;

					do
					{
						entity_yield(TASK_NO_DELAY);
					} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

					// Restore the animation.
					memcpy(local(anim), &local(tmp), sizeof(LogicAnimation) - 4);
					actor_setupAnimation2(local(obj), local(anim)->animId, local(anim));
					local(target)->flags &= 0xfffffff7;
				}
				msg = MSG_EXPLOSION;
			}
		}
		task_setMessage(msg);
		task_end;
	}

	void kellDragon_handleMsg(MessageType msg)
	{
		task_begin;
		if (msg == MSG_DAMAGE)
		{
			task_callTaskFunc(kellDragon_handleDamage);
		}
		else if (msg == MSG_EXPLOSION)
		{
			task_callTaskFunc(kellDragon_handleExplosion);
		}
		task_end;
	}

	JBool kellDragon_canSeePlayer(KellDragon* dragon)
	{
		SecObject* obj = dragon->logic.obj;
		JBool canSee = actor_canSeeObject(obj, s_playerObject);
		if (canSee)
		{
			PhysicsActor* physicsActor = &dragon->actor;
			physicsActor->lastPlayerPos.x = s_playerObject->posWS.x;
			physicsActor->lastPlayerPos.z = s_playerObject->posWS.z;
		}
		return canSee;
	}

	// Returns JTRUE if the new animation index has changed.
	JBool kellDragon_setAnimIndex(KellDragon* dragon, s32 baseAnimIndex)
	{
		SecObject* obj = dragon->logic.obj;
		PhysicsActor* physicsActor = &dragon->actor;
		fixed16_16 dist = distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, obj->posWS.x, obj->posWS.z);

		fixed16_16 value = 0;
		if (dist >= s_kellDragon_FarDist)
		{
			value = FIXED(3);
		}
		else if (dist >= s_kellDragon_MedDist)
		{
			value = FIXED(2);
		}
		else if (dist >= s_kellDragon_NearDist)
		{
			value = ONE_16;
		}
		fixed16_16 scale = mul16(div16(dragon->painLevel, FIXED(100)), FIXED(4));

		// if hp == max, result = 0; if hp = 0, result = 1
		fixed16_16 frac = div16(FIXED(180) - physicsActor->hp, FIXED(180));
		scale += mul16(frac, FIXED(4));

		// floor(2.0 * (painLevel/100.0 + (180 - hp)/180) + min(3, floor(dist/20.0)) + 0.5) + index
		s32 animIndex = baseAnimIndex + floor16(value + div16(scale, FIXED(2)) + HALF_16);
		animIndex = min(3, animIndex);
		if (animIndex != dragon->animIndex)
		{
			dragon->animIndex = animIndex;
			return JTRUE;
		}
		return JFALSE;
	}

	void kellDragon_setAnimFramerate(LogicAnimation* anim, s32 framerate)
	{
		anim->frameRate = framerate;
		anim->prevTick = s_frameTicks[framerate];
	}

	void kellDragon_computeTarget(SecObject* obj, KellDragon* dragon, ActorTarget* target)
	{
		fixed16_16 sinYaw, cosYaw;
		sinCosFixed(obj->yaw, &sinYaw, &cosYaw);

		target->flags |= 4;
		target->speedRotation = 6826;

		fixed16_16 animSpeed = s_kellDragonAnim[dragon->animIndex].speed;
		target->pos.x = obj->posWS.x + mul16(sinYaw, animSpeed);
		target->pos.z = obj->posWS.z + mul16(cosYaw, animSpeed);
		target->flags |= 1;
	}

	void kellDragon_handleState1(MessageType msg)
	{
		struct LocalContext
		{
			KellDragon* dragon;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			ActorTarget* target;
			u32 prevColTick;
			JBool yawAligned;
		};
		task_begin_ctx;

		local(dragon) = s_curDragon;
		local(obj) = local(dragon)->logic.obj;
		local(physicsActor) = &local(dragon)->actor;
		local(anim) = &local(physicsActor)->anim;
		local(target) = &local(physicsActor)->moveMod.target;
		local(prevColTick) = 0;
		local(yawAligned) = JFALSE;

		local(anim)->flags &= 0xfffffffe;
		actor_setupAnimation2(local(obj), 0, local(anim));

		task_localBlockBegin;
			s32 baseAnimIndex = (random(100) <= 60) ? 3 : 0;
			if (kellDragon_setAnimIndex(local(dragon), baseAnimIndex))
			{
				kellDragon_setAnimFramerate(local(anim), s_kellDragonAnim[local(dragon)->animIndex].framerate);
				local(target)->speed = s_kellDragonAnim[local(dragon)->animIndex].speed;
			}
		task_localBlockEnd;

		while (local(physicsActor)->state == 1)
		{
			do
			{
				entity_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleDamage);
				}
				else if (msg == MSG_EXPLOSION)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleExplosion);
				}
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != 1) { break; }

			if (!s_playerDying && kellDragon_canSeePlayer(local(dragon)))
			{
				JBool jump = JFALSE;
				if (local(obj)->yaw == local(target)->yaw)
				{
					if (!local(yawAligned))
					{
						jump = (random(100) <= 50) ? JTRUE : JFALSE;
					}
					local(yawAligned) = JTRUE;
				}
				else
				{
					local(yawAligned) = JFALSE;
				}

				fixed16_16 dy = TFE_Jedi::abs(local(obj)->posWS.y - s_playerObject->posWS.y);
				fixed16_16 dist = dy + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
				if (dist <= FIXED(20) && local(obj)->yaw == local(target)->yaw)
				{
					local(physicsActor)->state = 4;
				}
				else if (dist <= FIXED(55) && jump && local(obj)->yaw == local(target)->yaw)
				{
					if (dy <= FIXED(10))
					{
						local(physicsActor)->state = 3;
					}
				}
			}
			else if (!s_playerDying)
			{
				local(physicsActor)->state = 6;
			}
			if (local(physicsActor)->state != 1) { break; }

			if (kellDragon_setAnimIndex(local(dragon), 0))
			{
				kellDragon_setAnimFramerate(local(anim), s_kellDragonAnim[local(dragon)->animIndex].framerate);
				local(target)->speed = s_kellDragonAnim[local(dragon)->animIndex].speed;
			}

			local(target)->flags &= 0xfffffff7;
			if (actor_handleSteps(&local(physicsActor)->moveMod, local(target)))
			{
				actor_changeDirFromCollision(&local(physicsActor)->moveMod, local(target), &local(prevColTick));
				local(dragon)->retreat = JTRUE;
				local(target)->speedRotation = 11377;	// ~250 degrees per second.
				local(dragon)->nextTick = s_curTick + 72;
			}
			else if (local(dragon)->retreat)
			{
				if (s_curTick >= local(dragon)->nextTick)
				{
					local(dragon)->retreat = JFALSE;
					fixed16_16 dx = s_playerObject->posWS.x - local(obj)->posWS.x;
					fixed16_16 dz = s_playerObject->posWS.z - local(obj)->posWS.z;
					local(target)->yaw = vec2ToAngle(dx, dz);
					kellDragon_computeTarget(local(obj), local(dragon), local(target));
				}
			}
			else if (!s_playerDying)
			{
				fixed16_16 dx = s_playerObject->posWS.x - local(obj)->posWS.x;
				fixed16_16 dz = s_playerObject->posWS.z - local(obj)->posWS.z;
				local(target)->yaw = vec2ToAngle(dx, dz);
				kellDragon_computeTarget(local(obj), local(dragon), local(target));
			}
			else
			{
				local(target)->yaw = random(16338);
				kellDragon_computeTarget(local(obj), local(dragon), local(target));
			}
		}

		local(anim)->flags |= 2;
		task_end;
	}

	void kellDragon_handleRetreat(MessageType msg)
	{
		struct LocalContext
		{
			KellDragon* dragon;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			ActorTarget* target;
		};
		task_begin_ctx;

		local(dragon) = s_curDragon;
		local(obj) = local(dragon)->logic.obj;
		local(physicsActor) = &local(dragon)->actor;
		local(anim) = &local(physicsActor)->anim;
		local(target) = &local(physicsActor)->moveMod.target;
		local(anim)->flags &= 0xfffffffe;

		task_localBlockBegin;
			actor_setupAnimation2(local(obj), 0, local(anim));
			s32 baseAnimIndex = (random(100) <= 60) ? 3 : 0;
			if (kellDragon_setAnimIndex(local(dragon), baseAnimIndex))
			{
				kellDragon_setAnimFramerate(local(anim), s_kellDragonAnim[local(dragon)->animIndex].framerate);
				local(target)->speed = s_kellDragonAnim[local(dragon)->animIndex].speed;
			}
		task_localBlockEnd;

		while (local(physicsActor)->state == 2)
		{
			do
			{
				entity_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleDamage);
				}
				else if (msg == MSG_EXPLOSION)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleExplosion);
				}
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != 2) { break; }

			if (!local(dragon)->retreat)
			{
				fixed16_16 dx = s_playerObject->posWS.x - local(obj)->posWS.x;
				fixed16_16 dz = s_playerObject->posWS.z - local(obj)->posWS.z;
				local(target)->yaw = (vec2ToAngle(dx, dz) + 0x2000) & ANGLE_MASK;

				fixed16_16 sinYaw, cosYaw;
				sinCosFixed(local(target)->yaw, &sinYaw, &cosYaw);

				local(target)->flags |= 4;
				local(target)->speedRotation = 0;

				fixed16_16 animSpeed = s_kellDragonAnim[local(dragon)->animIndex].speed;
				local(target)->pos.x = local(obj)->posWS.x + mul16(sinYaw, animSpeed);
				local(target)->pos.z = local(obj)->posWS.z + mul16(cosYaw, animSpeed);
				local(target)->flags |= 1;

				local(dragon)->retreat = JTRUE;
				local(dragon)->nextTick = s_curTick + 72;
			}
			else if (s_curTick >= local(dragon)->nextTick)
			{
				local(physicsActor)->state = 1;
				local(dragon)->retreat = JFALSE;
			}
		}  // while (state == 2)

		local(anim)->flags |= 2;
		task_end;
	}

	void kellDragon_handleJumping(MessageType msg)
	{
		struct LocalContext
		{
			KellDragon* dragon;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			ActorTarget* target;
		};
		task_begin_ctx;

		local(dragon) = s_curDragon;
		local(obj) = local(dragon)->logic.obj;
		local(physicsActor) = &local(dragon)->actor;
		local(anim) = &local(physicsActor)->anim;
		local(target) = &local(physicsActor)->moveMod.target;
		
		while (local(physicsActor)->state == 3)
		{
			do
			{
				entity_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleDamage);
				}
				else if (msg == MSG_EXPLOSION)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleExplosion);
				}
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != 3) { break; }

			local(anim)->flags |= 1;
			actor_setupAnimation2(local(obj), 9, local(anim));
			local(anim)->frameRate = 8;
			sound_playCued(s_kellSound1, local(obj)->posWS);

			// Wait for the animation to finish playing.
			do
			{
				entity_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleDamage);
				}
				else if (msg == MSG_EXPLOSION)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleExplosion);
				}
			} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));
			
			task_localBlockBegin;
				local(target)->pos.x = s_playerObject->posWS.x;
				local(target)->pos.z = s_playerObject->posWS.z;
				local(target)->flags |= 1;

				fixed16_16 dx = s_playerObject->posWS.x - local(obj)->posWS.x;
				fixed16_16 dz = s_playerObject->posWS.z - local(obj)->posWS.z;
				local(target)->yaw = vec2ToAngle(dx, dz);
				local(obj)->yaw = local(target)->yaw;
				local(target)->flags |= 4;

				actor_jumpToTarget(local(physicsActor), local(obj), s_playerObject->posWS, FIXED(100), -455);
			task_localBlockEnd;

			// Wait for the actor to land.
			do
			{
				entity_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleDamage);
				}
				else if (msg == MSG_EXPLOSION)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleExplosion);
				}
			} while (msg != MSG_RUN_TASK || local(physicsActor)->vel.y != 0);

			// Remove any excess horizontal velocity and setup the next animation.
			local(target)->flags |= 8;
			local(physicsActor)->vel.x = 0;
			local(physicsActor)->vel.z = 0;
			actor_setupAnimation2(local(obj), 11, local(anim));
			local(anim)->frameRate = 8;
			sound_playCued(s_kellSound4, local(obj)->posWS);

			// Wait for the animation to finish playing.
			do
			{
				entity_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleDamage);
				}
				else if (msg == MSG_EXPLOSION)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleExplosion);
				}
			} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

			// Bite!
			fixed16_16 dy = TFE_Jedi::abs(local(obj)->posWS.y - s_playerObject->posWS.y);
			fixed16_16 dist = dy + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
			if (dist <= FIXED(20))
			{
				player_applyDamage(FIXED(20), 0, JTRUE);
			}
			local(physicsActor)->state = 1;
		}  // while (state == 3)
		task_end;
	}

	void kellDragon_handleState4(MessageType msg)
	{
		struct LocalContext
		{
			KellDragon* dragon;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			ActorTarget* target;
		};
		task_begin_ctx;

		local(dragon) = s_curDragon;
		local(obj) = local(dragon)->logic.obj;
		local(physicsActor) = &local(dragon)->actor;
		local(anim) = &local(physicsActor)->anim;
		local(target) = &local(physicsActor)->moveMod.target;

		while (local(physicsActor)->state == 4)
		{
			do
			{
				entity_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleDamage);
				}
				else if (msg == MSG_EXPLOSION)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleExplosion);
				}
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != 4) { break; }

			if (s_playerDying)
			{
				local(physicsActor)->state = 1;
				break;
			}

			local(anim)->flags |= 1;
			actor_setupAnimation2(local(obj), 1, local(anim));
			local(anim)->frameRate = 8;

			// Wait for the animation to finish playing.
			do
			{
				entity_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleDamage);
				}
				else if (msg == MSG_EXPLOSION)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleExplosion);
				}
			} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

			sound_playCued(s_kellSound4, local(obj)->posWS);
			fixed16_16 dy = TFE_Jedi::abs(local(obj)->posWS.y - s_playerObject->posWS.y);
			fixed16_16 dist = dy + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
			if (dist <= FIXED(20))
			{
				// Bite!
				player_applyDamage(FIXED(20), 0, JTRUE);
				if (random(100) <= 30)
				{
					local(physicsActor)->state = 2;
				}
			}
			else
			{
				local(physicsActor)->state = 1;
			}
		}

		local(target)->flags &= 0xfffffff7;
		task_end;
	}

	void kellDragon_handleDyingState(MessageType msg)
	{
		struct LocalContext
		{
			KellDragon* dragon;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			ActorTarget* target;
		};
		task_begin_ctx;

		local(dragon) = s_curDragon;
		local(obj) = local(dragon)->logic.obj;
		local(physicsActor) = &local(dragon)->actor;
		local(anim) = &local(physicsActor)->anim;
		local(target) = &local(physicsActor)->moveMod.target;

		local(target)->flags |= 8;
		sound_playCued(s_kellSound3, local(obj)->posWS);

		local(anim)->flags |= 1;
		actor_setupAnimation2(local(obj), 2, local(anim));
		local(anim)->frameRate = 8;

		// Wait for the death animation to finish.
		do
		{
			entity_yield(TASK_NO_DELAY);
		} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

		// Then wait until we can actually die...
		do
		{
			entity_yield(TASK_NO_DELAY);
		} while (msg != MSG_RUN_TASK || !actor_canDie(local(physicsActor)));

		task_localBlockBegin;
			RSector* sector = local(obj)->sector;
			if (sector->secHeight - 1 < 0)
			{
				SecObject* corpse = allocateObject();
				sprite_setData(corpse, local(obj)->wax);
				corpse->frame = 0;
				corpse->anim = 4;
				corpse->posWS = local(obj)->posWS;
				corpse->worldWidth = 0;
				corpse->worldHeight = 0;
				corpse->entityFlags |= (ETFLAG_CORPSE | ETFLAG_KEEP_CORPSE);
				sector_addObject(sector, corpse);
			}
			local(physicsActor)->alive = JFALSE;
			actor_handleBossDeath(local(physicsActor));
		task_localBlockEnd;
		task_end;
	}

	void kellDragon_handleState6(MessageType msg)
	{
		struct LocalContext
		{
			KellDragon* dragon;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			ActorTarget* target;

			JBool arrived;
			Tick  prevColTick;
			Tick  nextTick;
			Tick  nextTick2;
			Tick  delay;
			fixed16_16 speedRotation;
		};
		task_begin_ctx;

		local(dragon) = s_curDragon;
		local(obj) = local(dragon)->logic.obj;
		local(physicsActor) = &local(dragon)->actor;
		local(anim) = &local(physicsActor)->anim;
		local(target) = &local(physicsActor)->moveMod.target;

		local(arrived) = JTRUE;
		local(prevColTick) = 0;
		local(nextTick) = 0;
		local(nextTick2) = s_curTick + 0x1111;
		local(delay) = 72;
		local(speedRotation) = local(target)->speedRotation;

		local(anim)->flags &= 0xfffffffe;
		actor_setupAnimation2(local(obj), 0, local(anim));
		local(target)->flags &= 0xfffffff7;
		local(target)->speedRotation = 0x3000;

		while (local(physicsActor)->state == 6)
		{
			do
			{
				entity_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleDamage);
				}
				else if (msg == MSG_EXPLOSION)
				{
					s_curDragon = local(dragon);
					task_callTaskFunc(kellDragon_handleExplosion);
				}
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != 6 || s_playerDying) { break; }

			JBool canSee = actor_canSeeObject(local(obj), s_playerObject);
			if (canSee)
			{
				local(physicsActor)->lastPlayerPos.x = s_playerObject->posWS.x;
				local(physicsActor)->lastPlayerPos.z = s_playerObject->posWS.z;
				local(physicsActor)->state = 1;
				break;
			}

			JBool arrivedAtTarget = actor_arrivedAtTarget(local(target), local(obj));
			if (local(nextTick) < s_curTick || arrivedAtTarget)
			{
				local(arrived) = JTRUE;
				if (arrivedAtTarget)
				{
					local(nextTick2) = 0;
				}
			}

			if (local(arrived))
			{
				local(arrived) = JFALSE;
				vec2_fixed target;
				if (s_curTick < local(nextTick2))
				{
					target = local(physicsActor)->lastPlayerPos;
				}
				else
				{
					target = { s_eyePos.x, s_eyePos.z };
				}

				fixed16_16 absDx = TFE_Jedi::abs(s_playerObject->posWS.x - local(obj)->posWS.x);
				fixed16_16 dxTarget = local(obj)->posWS.x - target.x;
				fixed16_16 dzTarget = local(obj)->posWS.z - target.z;
				angle14_32 angleToTarget = vec2ToAngle(dxTarget, dzTarget);

				local(target)->pos.x = target.x;
				local(target)->pos.z = target.z;
				actor_offsetTarget(&local(target)->pos.x, &local(target)->pos.z, absDx>>2, absDx>>3, angleToTarget, 0xfff);
				local(target)->flags |= 1;

				dxTarget = local(target)->pos.x - local(obj)->posWS.x;
				dzTarget = local(target)->pos.z - local(obj)->posWS.z;
				local(target)->pitch = 0;
				local(target)->roll  = 0;
				local(target)->yaw   = vec2ToAngle(dxTarget, dzTarget);
				local(target)->flags |= 4;

				local(nextTick) = s_curTick + local(delay);
			}

			if (actor_handleSteps(&local(physicsActor)->moveMod, local(target)))
			{
				actor_changeDirFromCollision(&local(physicsActor)->moveMod, local(target), &local(prevColTick));

				local(delay) += 72;
				if (local(delay) > 1165) // ~8 seconds
				{
					local(delay) = 72;
				}
				local(nextTick) = s_curTick + local(delay);
			}
		}
		task_end;
	}

	void kellDragonTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			KellDragon* dragon;
			PhysicsActor* physicsActor;
		};
		task_begin_ctx;

		local(dragon) = (KellDragon*)task_getUserData();
		local(physicsActor) = &local(dragon)->actor;
		while (local(physicsActor)->alive)
		{
			msg = MSG_RUN_TASK;
			if (local(physicsActor)->state == 1)
			{
				s_curDragon = local(dragon);
				task_callTaskFunc(kellDragon_handleState1);
			}
			else if (local(physicsActor)->state == 2)
			{
				s_curDragon = local(dragon);
				task_callTaskFunc(kellDragon_handleRetreat);
			}
			else if (local(physicsActor)->state == 3)
			{
				s_curDragon = local(dragon);
				task_callTaskFunc(kellDragon_handleJumping);
			}
			else if (local(physicsActor)->state == 4)
			{
				s_curDragon = local(dragon);
				task_callTaskFunc(kellDragon_handleState4);
			}
			else if (local(physicsActor)->state == 5)
			{
				s_curDragon = local(dragon);
				task_callTaskFunc(kellDragon_handleDyingState);
			}
			else if (local(physicsActor)->state == 6)
			{
				s_curDragon = local(dragon);
				task_callTaskFunc(kellDragon_handleState6);
			}
			else
			{
				while (local(physicsActor)->state == 0)
				{
					do
					{
						entity_yield(145);
						s_curDragon = local(dragon);
						task_callTaskFunc(kellDragon_handleMsg);

						if (msg == MSG_DAMAGE || msg == MSG_EXPLOSION)
						{
							local(physicsActor)->state = 1;	// Pain State
							task_makeActive(local(physicsActor)->actorTask);
							task_yield(TASK_NO_DELAY);
						}
					} while (msg != MSG_RUN_TASK);
					if (local(physicsActor)->state == 0 && kellDragon_canSeePlayer(local(dragon)))
					{
						SecObject* obj = local(dragon)->logic.obj;
						sound_playCued(s_kellSound0, obj->posWS);
						local(physicsActor)->state = 1;
					}
				}  // while (state == 0)
			}
		}  // while (alive)

		// Dead:
		while (msg != MSG_RUN_TASK)
		{
			task_yield(TASK_NO_DELAY);
		}
		actor_removePhysicsActorFromWorld(local(physicsActor));
		deleteLogicAndObject((Logic*)local(dragon));
		level_free(local(dragon));

		task_end;
	}

	void kellDragonCleanupFunc(Logic* logic)
	{
		KellDragon* dragon = (KellDragon*)logic;

		actor_removePhysicsActorFromWorld(&dragon->actor);
		deleteLogicAndObject(logic);
		level_free(dragon);
		task_free(dragon->actor.actorTask);
	}

	Logic* kellDragon_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		if (!s_kellSound0)
		{
			s_kellSound0 = sound_load("kell-1.voc", SOUND_PRIORITY_MED5);
		}
		if (!s_kellSound2)
		{
			s_kellSound2 = sound_load("kell-8.voc", SOUND_PRIORITY_LOW0);
		}
		if (!s_kellSound4)
		{
			s_kellSound4 = sound_load("kell-5.voc", SOUND_PRIORITY_LOW0);
		}
		if (!s_kellSound1)
		{
			s_kellSound1 = sound_load("kelljump.voc", SOUND_PRIORITY_LOW0);
		}
		if (!s_kellSound3)
		{
			s_kellSound3 = sound_load("kell-7.voc", SOUND_PRIORITY_MED5);
		}

		KellDragon* dragon = (KellDragon*)level_alloc(sizeof(KellDragon));
		memset(dragon, 0, sizeof(KellDragon));

		// Give the name of the task a number so I can tell them apart when debugging.
		char name[32];
		sprintf(name, "KellDragon%d", s_dragonNum);
		s_dragonNum++;
		Task* task = createSubTask(name, kellDragonTaskFunc);
		task_setUserData(task, dragon);

		obj->entityFlags = ETFLAG_AI_ACTOR;
		PhysicsActor* physicsActor = &dragon->actor;
		physicsActor->alive = JTRUE;
		physicsActor->hp = FIXED(180);
		physicsActor->state = 0;
		physicsActor->actorTask = task;
		dragon->logic.obj = obj;
		dragon->animIndex = 0;
		dragon->painLevel = 0;
		dragon->painSndId = 0;
		dragon->retreat = JFALSE;

		actor_addPhysicsActorToWorld(physicsActor);
		physicsActor->moveMod.header.obj = obj;
		physicsActor->moveMod.physics.obj = obj;
		actor_setupSmartObj(&physicsActor->moveMod);

		obj->flags |= OBJ_FLAG_MOVABLE;

		CollisionInfo* physics = &physicsActor->moveMod.physics;
		physics->botOffset = 0x60000;
		physics->yPos = 0x80000;
		physics->width = obj->worldWidth;
		physicsActor->moveMod.collisionFlags |= 7;
		physics->height = obj->worldHeight + HALF_16;

		ActorTarget* target = &physicsActor->moveMod.target;
		target->flags &= 0xfffffff0;
		target->speedRotation = 43546;	// 956.8 degrees per second.
		target->speed = FIXED(10);

		LogicAnimation* anim = &physicsActor->anim;
		anim->frameRate = 5;
		anim->flags = (anim->flags | 2) & 0xfffffffe;
		anim->frameCount = ONE_16;
		anim->prevTick = 0;
		actor_setupAnimation2(obj, 5, anim);

		obj_addLogic(obj, &dragon->logic, LOGIC_DRAGON, task, kellDragonCleanupFunc);
		if (setupFunc)
		{
			*setupFunc = nullptr;
		}
		return (Logic*)dragon;
	}
}  // namespace TFE_DarkForces