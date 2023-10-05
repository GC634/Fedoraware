#include "Aimbot.h"
#include "../Vars.h"

#include "AimbotHitscan/AimbotHitscan.h"
#include "AimbotProjectile/AimbotProjectile.h"
#include "AimbotMelee/AimbotMelee.h"
#include "../Misc/Misc.h"

bool CAimbot::ShouldRun(CBaseEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	// Don't run while freecam is active
	if (G::FreecamActive) { return false; }

	// Don't run if aimbot is disabled
	if (!Vars::Aimbot::Global::Active.Value) { return false; }

	// Don't run in menus
	if (I::EngineVGui->IsGameUIVisible() || I::VGuiSurface->IsCursorVisible()) { return false; }

	// Don't run if we are frozen in place.
	if (G::Frozen) { return false; }

	// Check if we can shoot.
	if (!Vars::Aimbot::Global::DontWaitForShot.Value && (!G::IsAttacking && !G::WeaponCanAttack) && G::CurWeaponType != EWeaponType::MELEE)
	{
		return false;
	}

	// Check if we are alive and not in a state where we cannot aimbot.
	if (!pLocal->IsAlive()
		|| pLocal->IsTaunting()
		|| pLocal->IsBonked()
		|| pLocal->GetFeignDeathReady()
		|| pLocal->IsCloaked()
		|| pLocal->IsInBumperKart()
		|| pLocal->IsAGhost())
	{
		return false;
	}

	// Check if the weapon we are using can deal damage.
	if (pWeapon->GetWeaponID() == TF_WEAPON_BUILDER)
	{
		return true;
	}

	if (CTFWeaponInfo* sWeaponInfo = pWeapon->GetTFWeaponInfo())
	{
		WeaponData_t sWeaponData = sWeaponInfo->m_WeaponData[0];
		if (sWeaponData.m_nDamage < 1)
		{
			return false;
		}
	}

	return true;
}

void CAimbot::Run(CUserCmd* pCmd)
{
	// Reset the aimbot state.
	G::CurrentTargetIdx = 0;
	G::PredictedPos = Vec3();
	G::HitscanRunning = false;
	G::HitscanSilentActive = false;
	G::AimPos = Vec3();

	// Don't run the aimbot if we are moving or accelerating quickly.
	if (F::Misc.bMovementStopped || F::Misc.bFastAccel) { return; }

	// Get the local player and weapon entities.
	const auto pLocal = g_EntityCache.GetLocal();
	const auto pWeapon = g_EntityCache.GetWeapon();
	if (!pLocal || !pWeapon) { return; }

	// Don't run the aimbot if we cannot.
	if (!ShouldRun(pLocal, pWeapon)) { return; }

	// Check if we are using the sandvich.
	if (SandvichAimbot::bIsSandvich = SandvichAimbot::IsSandvich())
	{
		G::CurWeaponType = EWeaponType::HITSCAN;
	}

	// Run the appropriate aimbot function based on the weapon type.
	switch (G::CurWeaponType)
	{
		case EWeaponType::HITSCAN:
		{
			F::AimbotHitscan.Run(pLocal, pWeapon, pCmd);
			break;
		}

		case EWeaponType::PROJECTILE:
		{
			F::AimbotProjectile.Run(pLocal, pWeapon, pCmd);
			break;
		}

		case EWeaponType::MELEE:
		{
			F::AimbotMelee.Run(pLocal, pWeapon, pCmd);
			break;
		}

		default: break;
	}
}
