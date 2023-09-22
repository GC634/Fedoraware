#include "MovementSimulation.h"
#include "../../Backtrack/Backtrack.h"

// We'll use this to set the current player's command; without it, CGameMovement::CheckInterval will try to access a nullptr
static CUserCmd DummyCmd = {};

// Since we're going to call game functions, some entity data will be modified (we'll modify it too); we'll have to restore it after running
class CPlayerDataBackup
{
public:
    Vec3 m_vecOrigin = {};
    Vec3 m_vecVelocity = {};
    Vec3 m_vecBaseVelocity = {};
    Vec3 m_vecViewOffset = {};
    Vec3 m_vecWorldSpaceCenter = {};
    int m_hGroundEntity = 0;
    int m_fFlags = 0;
    float m_flDucktime = 0.0f;
    float m_flDuckJumpTime = 0.0f;
    bool m_bDucked = false;
    bool m_bDucking = false;
    bool m_bInDuckJump = false;
    float m_flModelScale = 0.0f;

public:
    void Store(CBaseEntity* pPlayer)
    {
        m_vecOrigin = pPlayer->m_vecOrigin();
        m_vecVelocity = pPlayer->m_vecVelocity();
        m_vecBaseVelocity = pPlayer->m_vecBaseVelocity();
        m_vecViewOffset = pPlayer->m_vecViewOffset();
        m_hGroundEntity = pPlayer->m_hGroundEntity();
        m_vecWorldSpaceCenter = pPlayer->GetWorldSpaceCenter();
        m_fFlags = pPlayer->m_fFlags();
        m_flDucktime = pPlayer->m_flDucktime();
        m_flDuckJumpTime = pPlayer->m_flDuckJumpTime();
        m_bDucked = pPlayer->m_bDucked();
        m_bDucking = pPlayer->m_bDucking();
        m_bInDuckJump = pPlayer->m_bInDuckJump();
        m_flModelScale = pPlayer->m_flModelScale();
    }

    void Restore(CBaseEntity* pPlayer)
    {
        pPlayer->m_vecOrigin() = m_vecOrigin;
        pPlayer->m_vecVelocity() = m_vecVelocity;
        pPlayer->m_vecBaseVelocity() = m_vecBaseVelocity;
        pPlayer->m_vecViewOffset() = m_vecViewOffset;
        pPlayer->m_hGroundEntity() = m_hGroundEntity;
        pPlayer->m_fFlags() = m_fFlags;
        pPlayer->m_flDucktime() = m_flDucktime;
        pPlayer->m_flDuckJumpTime() = m_flDuckJumpTime;
        pPlayer->m_bDucked() = m_bDucked;
        pPlayer->m_bDucking() = m_bDucking;
        pPlayer->m_bInDuckJump() = m_bInDuckJump;
        pPlayer->m_flModelScale() = m_flModelScale;
    }
};

static CPlayerDataBackup PlayerDataBackup = {};

void CMovementSimulation::SetupMoveData(CBaseEntity* pPlayer, CMoveData* pMoveData)
{
    if (!pPlayer || !pMoveData)
    {
        return;
    }

    bFirstRunTick = true;

    pMoveData->m_bFirstRunOfFunctions = false;
    pMoveData->m_bGameCodeMovedPlayer = false;
    pMoveData->m_nPlayerHandle = reinterpret_cast<IHandleEntity*>(pPlayer)->GetRefEHandle();
    pMoveData->m_vecVelocity = pPlayer->m_vecVelocity();
    pMoveData->m_vecAbsOrigin = pPlayer->m_vecOrigin();
    pMoveData->m_flMaxSpeed = pPlayer->TeamFortress_CalculateMaxSpeed();

    if (PlayerDataBackup.m_fFlags & FL_DUCKING)
    {
        pMoveData->m_flMaxSpeed *= 0.3333f;
    }

    pMoveData->m_flClientMaxSpeed = pMoveData->m_flMaxSpeed;

    // Need a better way to determine angles probably
    pMoveData->m_vecViewAngles = { 0.0f, Math::VelocityToAngles(pMoveData->m_vecVelocity).y, 0.0f };

    Vec3 vForward = {}, vRight = {};
    Math::AngleVectors(pMoveData->m_vecViewAngles, &vForward, &vRight, nullptr);

    pMoveData->m_flForwardMove = (pMoveData->m_vecVelocity.y - vRight.y / vRight.x * pMoveData->m_vecVelocity.x) / (vForward.y - vRight.y / vRight.x * vForward.x);
    pMoveData->m_flSideMove = (pMoveData->m_vecVelocity.x - vForward.x * pMoveData->m_flForwardMove) / vRight.x;
}

bool CMovementSimulation::Initialize(CBaseEntity* pPlayer)
{
    if (!pPlayer || pPlayer->deadflag())
    {
        return false;
    }

    // Set player
    m_pPlayer = pPlayer;

    // Set current command
    m_pPlayer->SetCurrentCmd(&DummyCmd);

    // Store player's data
    PlayerDataBackup.Store(m_pPlayer);

    // Store vars
    m_bOldInPrediction = I::Prediction->m_bInPrediction;
    m_bOldFirstTimePredicted = I::Prediction->m_bFirstTimePredicted;
    m_flOldFrametime = I::GlobalVars->frametime;
    bDontPredict = false;

    // The hacks that make it work
    {
        if (pPlayer->m_fFlags() & FL_DUCKING)
        {
            pPlayer->m_fFlags() &= ~FL_DUCKING; // Breaks origin's z if FL_DUCKING is not removed
            pPlayer->m_bDucked() = true; // Mins/maxs will be fine when ducking as long as m_bDucked is true
            pPlayer->m_flDucktime() = 0.0f;
            pPlayer->m_flDuckJumpTime() = 0.0f;
            pPlayer->m_bDucking() = false;
            pPlayer->m_bInDuckJump() = false;
        }

        if (pPlayer != g_EntityCache.GetLocal())
        {
            pPlayer->m_hGroundEntity() = -1; // Without this, non-local players get snapped to the floor
        }

        pPlayer->m_flModelScale() -= 0.03125f; // Fixes issues with corners

        if (pPlayer->m_fFlags() & FL_ONGROUND)
        {
            pPlayer->m_vecOrigin().z += 0.03125f; // To prevent getting stuck in the ground
        }

        // For some reason, if xy vel is zero, it doesn't predict
        if (fabsf(pPlayer->m_vecVelocity().x) < 0.01f)
        {
            pPlayer->m_vecVelocity().x = 0.015f;
        }

        if (fabsf(pPlayer->m_vecVelocity().y) < 0.01f)
        {
            pPlayer->m_vecVelocity().y = 0.015f;
        }
    }

    // Setup move data
    SetupMoveData(m_pPlayer, &m_MoveData);

    return true;
}

void CMovementSimulation::Restore()
{
    if (!m_pPlayer)
    {
        return;
    }

    m_pPlayer->SetCurrentCmd(nullptr);

    PlayerDataBackup.Restore(m_pPlayer);

    I::Prediction->m_bInPrediction = m_bOldInPrediction;
    I::Prediction->m_bFirstTimePredicted = m_bOldFirstTimePredicted;
    I::GlobalVars->frametime = m_flOldFrametime;

    m_pPlayer = nullptr;

    memset(&m_MoveData, 0, sizeof(CMoveData));
    memset(&PlayerDataBackup, 0, sizeof(CPlayerDataBackup));
}

void CMovementSimulation::FillVelocities()
{
    if (Vars::Aimbot::Projectile::StrafePredictionGround.Value || Vars::Aimbot::Projectile::StrafePredictionAir.Value)
    {
        for (const auto& pEntity : g_EntityCache.GetGroup(EGroupType::PLAYERS_ALL))
        {
            const int iEntIndex = pEntity->GetIndex();
            if (!pEntity->IsAlive() || pEntity->GetDormant() || pEntity->GetVelocity().IsZero())
            {
                m_Velocities[iEntIndex].clear();
                continue;
            }

            if (G::ChokeMap[pEntity->GetIndex()])
            { // Don't recache the same angle for lagging players.
                continue;
            }

            const Vec3 vVelocity = pEntity->GetVelocity();
            m_Velocities[iEntIndex].push_front(vVelocity);

            const size_t predSamples = Vars::Aimbot::Projectile::StrafePredictionSamples.Value;
            while (m_Velocities[iEntIndex].size() > predSamples)
            {
                m_Velocities[iEntIndex].pop_back();
            }
        }
    }
    else
    {
        m_Velocities.clear();
    }
}

bool CMovementSimulation::StrafePrediction()
{
    static float flAverageYaw = 0.f;
    static float flInitialYaw = 0.f;
    const bool shouldPredict = m_pPlayer->OnSolid() ? Vars::Aimbot::Projectile::StrafePredictionGround.Value : Vars::Aimbot::Projectile::StrafePredictionAir.Value;
    if (!shouldPredict)
    {
        return false;
    }

    // Fix in-air strafe prediction

    if (bFirstRunTick)
    { // We've already done the math, don't do it again
        flAverageYaw = 0.f;
        flInitialYaw = 0.f;
        bFirstRunTick = false; // If we fail the math here, don't try it again, it won't work.

        if (const auto& pLocal = g_EntityCache.GetLocal())
        {
            if (pLocal->GetAbsOrigin().DistTo(m_pPlayer->GetAbsOrigin()) > Vars::Aimbot::Projectile::StrafePredictionMaxDistance.Value)
            {
                return false;
            }
        }

        const int iEntIndex = m_pPlayer->GetIndex();

        const auto& mVelocityRecord = m_Velocities[iEntIndex];

        if (static_cast<int>(mVelocityRecord.size()) < 1)
        {
            return false;
        }

        const int iSamples = fmin(Vars::Aimbot::Projectile::StrafePredictionSamples.Value, mVelocityRecord.size());
        if (!iSamples)
        {
            return false;
        }

        flInitialYaw = m_MoveData.m_vecViewAngles.y; // Math::VelocityToAngles(m_MoveData.m_vecVelocity).y;
        float flCompareYaw = flInitialYaw;

        int i = 0;
        for (; i < iSamples; i++)
        {
            const float flRecordYaw = Math::VelocityToAngles(mVelocityRecord.at(i)).y;

            const float flDelta = RAD2DEG(Math::AngleDiffRad(DEG2RAD(flCompareYaw), DEG2RAD(flRecordYaw)));
            flAverageYaw += flDelta;

            flCompareYaw = flRecordYaw;
        }

        flAverageYaw /= i;

        while (flAverageYaw > 360.f)
        {
            flAverageYaw -= 360.f;
        }
        while (flAverageYaw < -360.f)
        {
            flAverageYaw += 360.f;
        }

        if (fabsf(flAverageYaw) < Vars::Aimbot::Projectile::StrafePredictionMinDifference.Value)
        {
            flAverageYaw = 0;
            return false;
        }

        const float flMaxDelta = (60.f / fmaxf((float)iSamples / 2.f, 1.f));

        if (fabsf(flAverageYaw) > flMaxDelta)
        {
            m_Velocities[m_pPlayer->GetIndex()].clear();
            return false;
        } // Ugly fix for sweaty pricks

        if (Vars::Debug::DebugInfo.Value)
        {
            Utils::ConLog("MovementSimulation", std::format("flAverageYaw calculated to {:.2f}", flAverageYaw).c_str(), { 83, 255, 83, 255 }, Vars::Debug::Logging.Value);
        }
    }
    if (flAverageYaw == 0.f)
    {
        return false;
    } // Fix

    flInitialYaw += flAverageYaw;
    m_MoveData.m_vecViewAngles.y = flInitialYaw;

    return true;
}

void CMovementSimulation::RunTick(CMoveData& moveDataOut, Vec3& m_vecAbsOrigin)
{
    if (!m_pPlayer || bDontPredict)
    {
        return;
    }

    if (m_pPlayer->GetClassID() != ETFClassID::CTFPlayer)
    {
        return;
    }

    // Make sure frametime and prediction vars are right
    I::Prediction->m_bInPrediction = true;
    I::Prediction->m_bFirstTimePredicted = false;
    I::GlobalVars->frametime = I::Prediction->m_bEnginePaused ? 0.0f : TICK_INTERVAL;

    if (!StrafePrediction())
    {
        m_MoveData.m_vecViewAngles = { 0.0f, Math::VelocityToAngles(m_MoveData.m_vecVelocity).y, 0.0f };
    }

    I::TFGameMovement->ProcessMovement(m_pPlayer, &m_MoveData);

    G::PredictionLines.emplace_back(m_MoveData.m_vecAbsOrigin, Math::GetRotatedPosition(m_MoveData.m_vecAbsOrigin, Math::VelocityToAngles(m_MoveData.m_vecVelocity).Length2D() + 90, Vars::Visuals::SeperatorLength.Value));

    moveDataOut = m_MoveData;
    m_vecAbsOrigin = m_MoveData.m_vecAbsOrigin;
}
