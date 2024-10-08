
#include "ArenaDalaranSewers.h"
#include "Battleground.h"
#include "Player.h"


enum BattlegroundDSObjectTypes
{
    BG_DS_OBJECT_DOOR_1 = 0,
    BG_DS_OBJECT_DOOR_2 = 1,
    BG_DS_OBJECT_WATER_1 = 2, // Collision
    BG_DS_OBJECT_WATER_2 = 3,
    BG_DS_OBJECT_BUFF_1 = 4,
    BG_DS_OBJECT_BUFF_2 = 5,
    BG_DS_OBJECT_MAX = 6
};

enum BattlegroundDSObjects
{
    BG_DS_OBJECT_TYPE_DOOR_1 = 192642,
    BG_DS_OBJECT_TYPE_DOOR_2 = 192643,
    BG_DS_OBJECT_TYPE_WATER_1 = 194395, // Collision
    BG_DS_OBJECT_TYPE_WATER_2 = 191877,
    BG_DS_OBJECT_TYPE_BUFF_1 = 184663,
    BG_DS_OBJECT_TYPE_BUFF_2 = 184664
};

enum BattlegroundDSCreatureTypes
{
    BG_DS_NPC_WATERFALL_KNOCKBACK = 0,
    BG_DS_NPC_PIPE_KNOCKBACK_1 = 1,
    BG_DS_NPC_PIPE_KNOCKBACK_2 = 2,
    BG_DS_NPC_MAX = 3
};

enum BattlegroundDSCreatures
{
    BG_DS_NPC_TYPE_WATER_SPOUT = 28567,
};

enum BattlegroundDSSpells
{
    BG_DS_SPELL_FLUSH = 57405, // Visual and target selector for the starting knockback from the pipe
    BG_DS_SPELL_FLUSH_KNOCKBACK = 61698, // Knockback effect for previous spell (triggered, not need to be casted)
    BG_DS_SPELL_WATER_SPOUT = 58873, // Knockback effect of the central waterfall
};

enum BattlegroundDSData
{ // These values are NOT blizzlike... need the correct data!
    BG_DS_WATERFALL_TIMER_MIN = 30000,
    BG_DS_WATERFALL_TIMER_MAX = 60000,
    BG_DS_WATERFALL_WARNING_DURATION = 5000,
    BG_DS_WATERFALL_DURATION = 30000,
    BG_DS_WATERFALL_KNOCKBACK_TIMER = 1500,

    BG_DS_PIPE_KNOCKBACK_FIRST_DELAY = 5000,
    BG_DS_PIPE_KNOCKBACK_DELAY = 3000,
    BG_DS_GET_OUT_TEXTURES = 3000,
    BG_DS_PIPE_KNOCKBACK_TOTAL_COUNT = 2,

    BG_DS_WATERFALL_STATUS_WARNING = 1, // Water starting to fall, but no LoS Blocking nor movement blocking
    BG_DS_WATERFALL_STATUS_ON = 2, // LoS and Movement blocking active
    BG_DS_WATERFALL_STATUS_OFF = 3,
};

ArenaDalaranSewers::ArenaDalaranSewers() : _waterfallTimer(0), _waterfallKnockbackTimer(0), _pipeKnockBackTimer(0), _getOutFromTexturesTimer(0), _pipeKnockBackCount(0), _waterfallStatus(0)
{
    BgObjects.resize(BG_DS_OBJECT_MAX);
    BgCreatures.resize(BG_DS_NPC_MAX);
}

ArenaDalaranSewers::~ArenaDalaranSewers() = default;

void ArenaDalaranSewers::PostUpdateImpl(uint32 diff)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    if (getPipeKnockBackCount() < BG_DS_PIPE_KNOCKBACK_TOTAL_COUNT)
    {
        if (getPipeKnockBackTimer() < diff)
        {
            for (uint32 i = BG_DS_NPC_PIPE_KNOCKBACK_1; i <= BG_DS_NPC_PIPE_KNOCKBACK_2; ++i)
                if (Creature* waterSpout = GetBgMap()->GetCreature(BgCreatures[i]))
                {
                    waterSpout->CastSpell(waterSpout, BG_DS_SPELL_FLUSH, true);
                    if (auto player = waterSpout->FindNearestPlayer(38.0f, true))
                        if (player->GetPositionZ() > 12.0f && !player->isGameMaster())
                        {
                            if (waterSpout->GetPositionX() < 1300.0f && waterSpout->GetPositionY() < 800.0f)
                                player->GetMotionMaster()->MoveJump(1271.39f, 765.94f, 7.2f, 30.0f, 10.0f);
                            else
                                player->GetMotionMaster()->MoveJump(1310.98f, 815.59f, 7.2f, 30.0f, 10.0f);
                        }
                }

            setPipeKnockBackCount(getPipeKnockBackCount() + 1);
            setPipeKnockBackTimer(BG_DS_PIPE_KNOCKBACK_DELAY);
        }
        else
            setPipeKnockBackTimer(getPipeKnockBackTimer() - diff);
    }

    if (getOutFromTexturesTimer() < diff)
    {
        float x = 1292.58f;
        float y = 790.22f;
        float z = 7.19f;
        for (auto const& itr : GetPlayers())
            if (auto player = ObjectAccessor::FindPlayer(GetBgMap(), itr.first))
                if (player->GetPositionZ() <= 2.0f)
                    player->NearTeleportTo(x, y, z, 0.0f);

        setgetOutFromTexturesTimer(BG_DS_GET_OUT_TEXTURES);
    }
    else
        setgetOutFromTexturesTimer(getOutFromTexturesTimer() - diff);

    if (getWaterFallStatus() == BG_DS_WATERFALL_STATUS_ON) // Repeat knockback while the waterfall still active
    {
        if (getWaterFallKnockbackTimer() < diff)
        {
            if (Creature* waterSpout = GetBgMap()->GetCreature(BgCreatures[BG_DS_NPC_WATERFALL_KNOCKBACK]))
                waterSpout->CastSpell(waterSpout, BG_DS_SPELL_WATER_SPOUT, true);

            setWaterFallKnockbackTimer(BG_DS_WATERFALL_KNOCKBACK_TIMER);
        }
        else
            setWaterFallKnockbackTimer(getWaterFallKnockbackTimer() - diff);
    }

    if (getWaterFallTimer() < diff)
    {
        if (getWaterFallStatus() == BG_DS_WATERFALL_STATUS_OFF) // Add the water
        {
            DoorClose(BG_DS_OBJECT_WATER_2);
            setWaterFallTimer(BG_DS_WATERFALL_WARNING_DURATION);
            setWaterFallStatus(BG_DS_WATERFALL_STATUS_WARNING);
        }
        else if (getWaterFallStatus() == BG_DS_WATERFALL_STATUS_WARNING) // Active collision and start knockback timer
        {
            if (GameObject* gob = GetBgMap()->GetGameObject(BgObjects[BG_DS_OBJECT_WATER_1]))
                gob->SetGoState(GO_STATE_READY);

            setWaterFallTimer(BG_DS_WATERFALL_DURATION);
            setWaterFallStatus(BG_DS_WATERFALL_STATUS_ON);
            setWaterFallKnockbackTimer(BG_DS_WATERFALL_KNOCKBACK_TIMER);
        }
        else //if (getWaterFallStatus() == BG_DS_WATERFALL_STATUS_ON) // Remove collision and water
        {
            // turn off collision
            if (GameObject* gob = GetBgMap()->GetGameObject(BgObjects[BG_DS_OBJECT_WATER_1]))
                gob->SetGoState(GO_STATE_ACTIVE);

            DoorOpen(BG_DS_OBJECT_WATER_2);
            setWaterFallTimer(urand(BG_DS_WATERFALL_TIMER_MIN, BG_DS_WATERFALL_TIMER_MAX));
            setWaterFallStatus(BG_DS_WATERFALL_STATUS_OFF);
        }
    }
    else
        setWaterFallTimer(getWaterFallTimer() - diff);

    Arena::PostUpdateImpl(diff);
}

void ArenaDalaranSewers::StartingEventCloseDoors()
{
    for (uint32 i = BG_DS_OBJECT_DOOR_1; i <= BG_DS_OBJECT_DOOR_2; ++i)
        SpawnBGObject(i, RESPAWN_IMMEDIATELY);

    Arena::StartingEventCloseDoors();
}

void ArenaDalaranSewers::StartingEventOpenDoors()
{
    for (uint32 i = BG_DS_OBJECT_DOOR_1; i <= BG_DS_OBJECT_DOOR_2; ++i)
        DoorOpen(i);

    for (uint32 i = BG_DS_OBJECT_BUFF_1; i <= BG_DS_OBJECT_BUFF_2; ++i)
        SpawnBGObject(i, 60);

    setWaterFallTimer(urand(BG_DS_WATERFALL_TIMER_MIN, BG_DS_WATERFALL_TIMER_MAX));
    setWaterFallStatus(BG_DS_WATERFALL_STATUS_OFF);

    setPipeKnockBackTimer(BG_DS_PIPE_KNOCKBACK_FIRST_DELAY);
    setPipeKnockBackCount(0);

    SpawnBGObject(BG_DS_OBJECT_WATER_2, RESPAWN_IMMEDIATELY);
    DoorOpen(BG_DS_OBJECT_WATER_2);

    // Turn off collision
    if (GameObject* gob = GetBgMap()->GetGameObject(BgObjects[BG_DS_OBJECT_WATER_1]))
        gob->SetGoState(GO_STATE_ACTIVE);

    // Remove effects of Demonic Circle Summon
    for (auto const& itr : GetPlayers())
        if (Player* player = ObjectAccessor::FindPlayer(GetBgMap(), itr.first))
            if (player->HasAura(48018))
                player->RemoveAurasDueToSpell(48018);

    Arena::StartingEventOpenDoors();
}

void ArenaDalaranSewers::_CheckPositions(uint32 diff)
{
    for (auto const& itr : GetPlayers())
    {
        Player* player = ObjectAccessor::FindPlayer(GetBgMap(), itr.first);
        if (!player)
            continue;

        if (player->IsInAreaTriggerRadius(5347) || player->IsInAreaTriggerRadius(5348))
        {
            if (player->HasAura(48018)) // Remove effects of Demonic Circle Summon
                player->RemoveAurasDueToSpell(48018);

            // Someone has get back into the pipes and the knockback has already been performed, so we reset the knockback count for kicking the player again into the arena.
            if (getPipeKnockBackCount() >= BG_DS_PIPE_KNOCKBACK_TOTAL_COUNT)
                setPipeKnockBackCount(0);
        }
    }

    Battleground::_CheckPositions(diff);
}

bool ArenaDalaranSewers::SetupBattleground()
{
    if (!AddObject(BG_DS_OBJECT_DOOR_1, BG_DS_OBJECT_TYPE_DOOR_1, 1350.95f, 817.2f, 20.8096f, 3.15f, 0, 0, 0.99627f, 0.0862864f, RESPAWN_IMMEDIATELY) ||
        !AddObject(BG_DS_OBJECT_DOOR_2, BG_DS_OBJECT_TYPE_DOOR_2, 1232.65f, 764.913f, 20.0729f, 6.3f, 0, 0, 0.0310211f, -0.999519f, RESPAWN_IMMEDIATELY) ||
        !AddObject(BG_DS_OBJECT_WATER_1, BG_DS_OBJECT_TYPE_WATER_1, 1291.56f, 790.837f, 7.1f, 3.14238f, 0, 0, 0.694215f, -0.719768f, 120) ||
        !AddObject(BG_DS_OBJECT_WATER_2, BG_DS_OBJECT_TYPE_WATER_2, 1291.56f, 790.837f, 7.1f, 3.14238f, 0, 0, 0.694215f, -0.719768f, 120) ||
        !AddObject(BG_DS_OBJECT_BUFF_1, BG_DS_OBJECT_TYPE_BUFF_1, 1291.7f, 813.424f, 7.11472f, 4.64562f, 0, 0, 0.730314f, -0.683111f, 120) ||
        !AddObject(BG_DS_OBJECT_BUFF_2, BG_DS_OBJECT_TYPE_BUFF_2, 1291.7f, 768.911f, 7.11472f, 1.55194f, 0, 0, 0.700409f, 0.713742f, 120) ||
        !AddCreature(BG_DS_NPC_TYPE_WATER_SPOUT, BG_DS_NPC_WATERFALL_KNOCKBACK, 0, 1292.587f, 790.2205f, 7.19796f, 3.054326f, RESPAWN_IMMEDIATELY) ||
        !AddCreature(BG_DS_NPC_TYPE_WATER_SPOUT, BG_DS_NPC_PIPE_KNOCKBACK_1, 0, 1369.977f, 817.2882f, 16.08718f, 3.106686f, RESPAWN_IMMEDIATELY) ||
        !AddCreature(BG_DS_NPC_TYPE_WATER_SPOUT, BG_DS_NPC_PIPE_KNOCKBACK_2, 0, 1212.833f, 765.3871f, 16.09484f, 0.0f, RESPAWN_IMMEDIATELY))
    {
        TC_LOG_ERROR("sql.sql", "BatteGroundDS: Failed to spawn some object!");
        return false;
    }

    return true;
}
