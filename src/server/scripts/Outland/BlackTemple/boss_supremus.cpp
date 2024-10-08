/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2006-2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "PassiveAI.h"
#include "black_temple.h"

enum Says
{
    EMOTE_NEW_TARGET = 0,
    EMOTE_PUNCH_GROUND,
    EMOTE_GROUND_CRACK
};

//Spells
#define SPELL_MOLTEN_PUNCH          40126
#define SPELL_HATEFUL_STRIKE        41926
#define SPELL_MOLTEN_FLAME          40980
#define SPELL_VOLCANIC_ERUPTION     40117
#define SPELL_VOLCANIC_SUMMON       40276
#define SPELL_BERSERK               45078

#define CREATURE_VOLCANO            23085
#define CREATURE_STALKER            23095

#define PHASE_STRIKE    1
#define PHASE_CHASE     2

#define EVENT_BERSERK           1
#define EVENT_SWITCH_PHASE      2
#define EVENT_FLAME             3
#define EVENT_VOLCANO           4
#define EVENT_SWITCH_TARGET     5
#define EVENT_HATEFUL_STRIKE    6

#define GCD_CAST    1

class molten_flame : public CreatureScript
{
public:
    molten_flame() : CreatureScript("molten_flame") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new molten_flameAI (creature);
    }

    struct molten_flameAI : public NullCreatureAI
    {
        molten_flameAI(Creature* creature) : NullCreatureAI(creature) {}

        void InitializeAI()
        {
            float x, y, z;
            me->GetNearPoint(me, x, y, z, 1, 100, float(M_PI*2*rand_norm()));
            me->GetMotionMaster()->MovePoint(0, x, y, z);
            me->SetVisible(false);
            me->CastSpell(me, SPELL_MOLTEN_FLAME, true);
        }
    };

};

class boss_supremus : public CreatureScript
{
public:
    boss_supremus() : CreatureScript("boss_supremus") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_supremusAI (creature);
    }

    struct boss_supremusAI : public ScriptedAI
    {
        boss_supremusAI(Creature* creature) : ScriptedAI(creature), summons(me)
        {
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;
        EventMap events;
        SummonList summons;
        uint32 phase;

        void Reset()
        {
            if (instance)
            {
                if (me->IsAlive())
                {
                    instance->SetData(DATA_SUPREMUSEVENT, NOT_STARTED);
                    //ToggleDoors(true);
                }
                //else ToggleDoors(false);
            }

            phase = 0;

            events.Reset();
            summons.DespawnAll();
        }

        void EnterCombat(Unit* /*who*/)
        {
            if (instance)
                instance->SetData(DATA_SUPREMUSEVENT, IN_PROGRESS);

            ChangePhase();
            events.ScheduleEvent(EVENT_BERSERK, 900000, GCD_CAST);
            events.ScheduleEvent(EVENT_FLAME, 20000, GCD_CAST);
        }

        void ChangePhase()
        {
            if (!phase || phase == PHASE_CHASE)
            {
                phase = PHASE_STRIKE;
                DummyEntryCheckPredicate pred;
                summons.DoAction(EVENT_VOLCANO, pred);
                events.ScheduleEvent(EVENT_HATEFUL_STRIKE, 5000, GCD_CAST, PHASE_STRIKE);
                me->SetSpeed(MOVE_RUN, 1.2f);
                me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, false);
                me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, false);
            }
            else
            {
                phase = PHASE_CHASE;
                events.ScheduleEvent(EVENT_VOLCANO, 5000, GCD_CAST, PHASE_CHASE);
                events.ScheduleEvent(EVENT_SWITCH_TARGET, 10000, 0, PHASE_CHASE);
                me->SetSpeed(MOVE_RUN, 0.9f);
                me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, true);
                me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, true);
            }
            DoResetThreat();
            DoZoneInCombat();
            events.SetPhase(phase);
            events.ScheduleEvent(EVENT_SWITCH_PHASE, 60000, GCD_CAST);
        }

        void JustDied(Unit* /*killer*/)
        {
            if (instance)
            {
                instance->SetData(DATA_SUPREMUSEVENT, DONE);
                instance->HandleGameObject(instance->GetGuidData(DATA_GAMEOBJECT_SUPREMUS_DOORS), true);
            }
            summons.DespawnAll();
        }

        void JustSummoned(Creature* summon)
        {
            summons.Summon(summon);
        }

        void SummonedCreatureDespawn(Creature* summon)
        {
            summons.Despawn(summon);
        }

        Unit* CalculateHatefulStrikeTarget()
        {
            uint32 health = 0;
            Unit* target = NULL;

            std::list<HostileReference*>& m_threatlist = me->getThreatManager().getThreatList();
            std::list<HostileReference*>::const_iterator i = m_threatlist.begin();
            for (i = m_threatlist.begin(); i!= m_threatlist.end(); ++i)
            {
                Unit* unit = Unit::GetUnit(*me, (*i)->getUnitGuid());
                if (unit && me->IsWithinMeleeRange(unit))
                {
                    if (unit->GetHealth() > health)
                    {
                        health = unit->GetHealth();
                        target = unit;
                    }
                }
            }

            return target;
        }

        void UpdateAI(uint32 diff)
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_BERSERK:
                        DoCast(me, SPELL_BERSERK, true);
                        break;
                    case EVENT_FLAME:
                        DoCast(me, SPELL_MOLTEN_PUNCH);
                        events.DelayEvents(1500, GCD_CAST);
                        events.ScheduleEvent(EVENT_FLAME, 20000, GCD_CAST);
                        break;
                    case EVENT_HATEFUL_STRIKE:
                        if (Unit* target = CalculateHatefulStrikeTarget())
                            DoCast(target, SPELL_HATEFUL_STRIKE);
                        events.DelayEvents(1000, GCD_CAST);
                        events.ScheduleEvent(EVENT_HATEFUL_STRIKE, 5000, GCD_CAST, PHASE_STRIKE);
                        break;
                    case EVENT_SWITCH_TARGET:
                        if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 1, 100, true))
                        {
                            DoResetThreat();
                            me->AddThreat(target, 5000000.0f);
                            DoScriptText(EMOTE_NEW_TARGET, me);
                        }
                        events.ScheduleEvent(EVENT_SWITCH_TARGET, 10000, 0, PHASE_CHASE);
                        break;
                    case EVENT_VOLCANO:
                    {
                        Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 999, true);
                        if (!target) target = me->getVictim();
                        if (target)
                        {
                            //DoCast(target, SPELL_VOLCANIC_SUMMON);//movement bugged
                            me->SummonCreature(CREATURE_VOLCANO, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 30000);
                            Talk(EMOTE_GROUND_CRACK);
                            events.DelayEvents(1500, GCD_CAST);
                        }
                        events.ScheduleEvent(EVENT_VOLCANO, 10000, GCD_CAST, PHASE_CHASE);
                        return;
                    }
                    case EVENT_SWITCH_PHASE:
                        ChangePhase();
                        break;
                }
            }

            DoMeleeAttackIfReady();
        }
    };

};

class npc_volcano : public CreatureScript
{
public:
    npc_volcano() : CreatureScript("npc_volcano") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_volcanoAI (creature);
    }

    struct npc_volcanoAI : public Scripted_NoMovementAI
    {
        npc_volcanoAI(Creature* creature) : Scripted_NoMovementAI(creature) {}

        void Reset()
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            //DoCast(me, SPELL_VOLCANIC_ERUPTION);
            me->SetReactState(REACT_PASSIVE);
            wait = 3000;
        }
        uint32 wait;

        void EnterCombat(Unit* /*who*/) {}

        void MoveInLineOfSight(Unit* /*who*/) {}

        void DoAction(const int32 /*info*/)
        {
            me->RemoveAura(SPELL_VOLCANIC_ERUPTION);
        }

        void UpdateAI(uint32 diff)
        {
            if (wait <= diff)//wait 3secs before casting
            {
                DoCast(me, SPELL_VOLCANIC_ERUPTION);
                wait = 60000;
            }
            else wait -= diff;
        }

    };

};

void AddSC_boss_supremus()
{
    new boss_supremus();
    new molten_flame();
    new npc_volcano();
}
