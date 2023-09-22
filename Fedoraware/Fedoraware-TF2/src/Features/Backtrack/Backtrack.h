#pragma once
#include "../Feature.h"

#pragma warning(disable: 4091)

class IncomingSequence {
public:
    int inReliableState;
    int sequenceNr;
    float curTime;

    IncomingSequence(int inState, int seqNr, float time) {
        inReliableState = inState;
        sequenceNr = seqNr;
        curTime = time;
    }
};

using BoneMatrices = struct {
    float boneMatrix[128][3][4];
};

struct TickRecord {
    float simTime = 0.f;
    float createTime = 0.f;
    int tickCount = 0;
    bool onShot = false;
    BoneMatrices boneMatrix{};
    Vec3 origin = {};
    Vec3 angles = {};
};

enum class BacktrackMode {
    ALL,           // Iterates through every tick (may be slow)
    LAST,          // Last tick
    PREFERONSHOT   // Prefers on-shot records, otherwise last
};

class Backtrack {
    const Color_t BT_LOG_COLOUR{ 150, 0, 212, 255 };

    // Logic
    bool isTracked(const TickRecord& record);
    bool isSimulationReliable(CBaseEntity* pEntity);
    bool isEarly(CBaseEntity* pEntity);
    bool withinRewindEx(const TickRecord& record, const float compTime);

    // Utils
    void cleanRecords();
    void makeRecords();
    std::optional<TickRecord> getHitRecord(CUserCmd* pCmd, CBaseEntity* pEntity, Vec3 vAngles, Vec3 vPos);
    void updateDatagram();
    float getLatency();

    // Data
    std::unordered_map<CBaseEntity*, std::deque<TickRecord>> records;
    std::unordered_map<int, bool> didShoot;
    int lastCreationTick = 0;

    // Data - fake latency
    std::deque<IncomingSequence> sequences;
    float latencyRampup = 0.f;
    int lastInSequence = 0;

public:
    bool withinRewind(const TickRecord& record, const float delay = 0.f);
    bool canHitOriginal(CBaseEntity* pEntity);
    void playerHurt(CGameEvent* pEvent); // Called on player_hurt event
    void resolverUpdate(CBaseEntity* pEntity); // Omitted 'omfg' for clarity
    void restart(); // Called whenever
    void frameStageNotify(); // Called in FrameStageNotify
    void reportShot(int index);
    std::deque<TickRecord>* getRecords(CBaseEntity* pEntity);
    std::optional<TickRecord> aimbot(CBaseEntity* pEntity, BacktrackMode mode, int hitbox);
    std::optional<TickRecord> getLastRecord(CBaseEntity* pEntity);
    std::optional<TickRecord> getFirstRecord(CBaseEntity* pEntity);
    std::optional<TickRecord> run(CUserCmd* pCmd); // Returns a valid record
    void adjustPing(INetChannel* netChannel); // Blah
    bool fakeLatency = false;
};

ADD_FEATURE(Backtrack, Backtrack)
