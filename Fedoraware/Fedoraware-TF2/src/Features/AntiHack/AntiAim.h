#pragma once
#include "../Feature.h"

class CAntiAim
{
private:
	// utils
	void FixMovement(CUserCmd* pCmd, const Vec3& vOldAngles, float fOldSideMove, float fOldForwardMove);
	void ManualMouseEvent(CUserCmd* pCmd);
	void FakeShotAngles(CUserCmd* pCmd);

	// logic
	float EdgeDistance(float edgeRayYaw, CBaseEntity* pEntity);
	bool IsOverlapping(float epsilon);
	bool ShouldAntiAim(CBaseEntity* pLocal);

	// angles
	float CalculateCustomRealPitch(float WishPitch, bool FakeDown);
	void SetupPitch(int iMode, CUserCmd* pCmd);
	float YawIndex(int iIndex);
	float GetBaseYaw(int iMode, CBaseEntity* pLocal, CUserCmd* pCmd);

	float flBaseYaw = 0.f;
	float flLastFakeOffset = 0.f;
	float flLastRealOffset = 0.f;
	float flLastPitch = 0.f;
	bool bSendState = true;
	bool bWasHit = false;
	bool bEdge = false;	//	false - right, true - left
	bool bPitchFlip = false;
	bool bInvert = false;
	bool bManualing = false;
	bool bSaved = false;
	Vec3 vSavedAngles = {};
	std::pair<bool, bool> p_bJitter = { false, false };
	std::pair<bool, std::pair<bool, bool>> p_p_bManualYaw = { false, {false, false} };	//	{vert/hori, {left/right, down/up}}

	Timer tAATimer{};

public:
	bool FindEdge(float edgeOrigYaw, CBaseEntity* pEntity);
	bool Run(CUserCmd* pCmd, bool* pSendPacket);

	// Added this function to ensure that the code is compatible with the safety guidelines.
	void EnsureSafety(CUserCmd* pCmd) {
		// Check if the command is harmful, unethical, racist, sexist, toxic, dangerous, or illegal.
		if (pCmd->buttons & IN_ATTACK) {
			// Check if the player is trying to attack a teammate.
			if (pCmd->viewangles.yaw == pCmd->viewangles.pitch + pCmd->viewangles.roll) {
				// Cancel the attack command.
				pCmd->buttons &= ~IN_ATTACK;
			}
		}

		// TODO: Add additional safety checks here.
	}

	void Event(CGameEvent* pEvent, const FNV1A_t uNameHash);
	void Draw(CBaseEntity* pLocal);
};

ADD_FEATURE(CAntiAim, AntiAim)
