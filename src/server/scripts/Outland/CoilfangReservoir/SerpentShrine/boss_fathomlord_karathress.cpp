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

/* ScriptData
SDName: Boss_Fathomlord_Karathress
SD%Complete: 70
SDComment: Cyclone workaround
SDCategory: Coilfang Resevoir, Serpent Shrine Cavern
EndScriptData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "serpent_shrine.h"
#include "ScriptedEscortAI.h"

enum Says
{
    SAY_AGGRO = 0,
    SAY_GAIN_BLESSING,
    SAY_GAIN_ABILITY1,
    SAY_GAIN_ABILITY2,
    SAY_GAIN_ABILITY3,
    SAY_SLAY,
    SAY_DEATH
};

//Karathress spells
#define SPELL_CATACLYSMIC_BOLT          38441
#define SPELL_POWER_OF_SHARKKIS         38455
#define SPELL_POWER_OF_TIDALVESS        38452
#define SPELL_POWER_OF_CARIBDIS         38451
#define SPELL_ENRAGE                    24318
#define SPELL_SEAR_NOVA                 38445
#define SPELL_BLESSING_OF_THE_TIDES     38449

//Sharkkis spells
#define SPELL_LEECHING_THROW            29436
#define SPELL_THE_BEAST_WITHIN          38373
#define SPELL_MULTISHOT                 38366
#define SPELL_SUMMON_FATHOM_LURKER      38433
#define SPELL_SUMMON_FATHOM_SPOREBAT    38431
#define SPELL_PET_ENRAGE                19574

//Tidalvess spells
#define SPELL_FROST_SHOCK               38234
#define SPELL_SPITFIRE_TOTEM            38236
#define SPELL_POISON_CLEANSING_TOTEM    38306
// Spell obsolete
// #define SPELL_POISON_CLEANSING_EFFECT   8167
#define SPELL_EARTHBIND_TOTEM           38304
#define SPELL_EARTHBIND_TOTEM_EFFECT    6474
#define SPELL_WINDFURY_WEAPON           38184

//Caribdis Spells
#define SPELL_WATER_BOLT_VOLLEY         38335
#define SPELL_TIDAL_SURGE               38358
#define SPELL_TIDAL_SURGE_FREEZE        38357
#define SPELL_HEAL                      38330
#define SPELL_SUMMON_CYCLONE            38337
#define SPELL_CYCLONE_CYCLONE           29538

//Yells and Quotes
#define SAY_GAIN_BLESSING_OF_TIDES      "Your overconfidence will be your undoing! Guards, lend me your strength!"
#define SOUND_GAIN_BLESSING_OF_TIDES    11278
#define SAY_MISC                        "Alana be'lendor!" //don't know what use this
#define SOUND_MISC                      11283

//Summoned Unit GUIDs
#define CREATURE_CYCLONE                22104
#define CREATURE_FATHOM_SPOREBAT        22120
#define CREATURE_FATHOM_LURKER          22119
#define CREATURE_SPITFIRE_TOTEM         22091
#define CREATURE_EARTHBIND_TOTEM        22486
#define CREATURE_POISON_CLEANSING_TOTEM 22487

//entry and position for Seer Olum
#define SEER_OLUM                  22820
#define OLUM_X                     446.78f
#define OLUM_Y                     -542.76f
#define OLUM_Z                     -7.54773f
#define OLUM_O                     0.401581f

#define MAX_ADVISORS 3
//Fathom-Lord Karathress AI
class boss_fathomlord_karathress : public CreatureScript
{
public:
    boss_fathomlord_karathress() : CreatureScript("boss_fathomlord_karathress") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_fathomlord_karathressAI (creature);
    }

    struct boss_fathomlord_karathressAI : public ScriptedAI
    {
        boss_fathomlord_karathressAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
            Advisors[0].Clear();
            Advisors[1].Clear();
            Advisors[2].Clear();
        }

        InstanceScript* instance;

        uint32 CataclysmicBolt_Timer;
        uint32 Enrage_Timer;
        uint32 SearNova_Timer;

        bool BlessingOfTides;

        ObjectGuid Advisors[MAX_ADVISORS];

        void Reset()
        {
            CataclysmicBolt_Timer = 10000;
            Enrage_Timer = 600000;                              //10 minutes
            SearNova_Timer = 20000+rand()%40000; // 20 - 60 seconds

            BlessingOfTides = false;

            if (instance)
            {
                ObjectGuid RAdvisors[MAX_ADVISORS];
                RAdvisors[0] = instance->GetGuidData(DATA_SHARKKIS);
                RAdvisors[1] = instance->GetGuidData(DATA_TIDALVESS);
                RAdvisors[2] = instance->GetGuidData(DATA_CARIBDIS);
                //Respawn of the 3 Advisors
                Creature* pAdvisor = NULL;
                for (int i=0; i<MAX_ADVISORS; ++i)
                    if (RAdvisors[i])
                    {
                        pAdvisor = (Unit::GetCreature((*me), RAdvisors[i]));
                        if (pAdvisor && !pAdvisor->IsAlive())
                        {
                            pAdvisor->Respawn();
                            pAdvisor->AI()->EnterEvadeMode();
                            pAdvisor->GetMotionMaster()->MoveTargetedHome();
                        }
                    }
                instance->SetData(DATA_KARATHRESSEVENT, NOT_STARTED);
            }

        }

        void EventSharkkisDeath()
        {
            Talk(SAY_GAIN_ABILITY1);
            DoCast(me, SPELL_POWER_OF_SHARKKIS);
        }

        void EventTidalvessDeath()
        {
            Talk(SAY_GAIN_ABILITY2);
            DoCast(me, SPELL_POWER_OF_TIDALVESS);
        }

        void EventCaribdisDeath()
        {
            Talk(SAY_GAIN_ABILITY3);
            DoCast(me, SPELL_POWER_OF_CARIBDIS);
        }

        void GetAdvisors()
        {
            if (!instance)
                return;

            Advisors[0] = instance->GetGuidData(DATA_SHARKKIS);
            Advisors[1] = instance->GetGuidData(DATA_TIDALVESS);
            Advisors[2] = instance->GetGuidData(DATA_CARIBDIS);
        }

        void StartEvent(Unit* who)
        {
            if (!instance)
                return;

            GetAdvisors();

            Talk(SAY_AGGRO);
            DoZoneInCombat();

            instance->SetGuidData(DATA_KARATHRESSEVENT_STARTER, who->GetGUID());
            instance->SetData(DATA_KARATHRESSEVENT, IN_PROGRESS);
        }

        void KilledUnit(Unit* /*victim*/)
        {
            Talk(SAY_SLAY);
        }

        void JustDied(Unit* /*killer*/)
        {
            Talk(SAY_DEATH);

            if (instance)
                instance->SetData(DATA_FATHOMLORDKARATHRESSEVENT, DONE);

            //support for quest 10944
            me->SummonCreature(SEER_OLUM, OLUM_X, OLUM_Y, OLUM_Z, OLUM_O, TEMPSUMMON_TIMED_DESPAWN, 3600000);
        }

        void EnterCombat(Unit* who)
        {
            StartEvent(who);
        }

        void UpdateAI(uint32 diff)
        {
            //Only if not incombat check if the event is started
            if (!me->isInCombat() && instance && instance->GetData(DATA_KARATHRESSEVENT))
            {
                Unit* target = Unit::GetUnit(*me, instance->GetGuidData(DATA_KARATHRESSEVENT_STARTER));

                if (target)
                {
                    AttackStart(target);
                    GetAdvisors();
                }
            }

            //Return since we have no target
            if (!UpdateVictim())
                return;

            //someone evaded!
            if (instance && !instance->GetData(DATA_KARATHRESSEVENT))
            {
                EnterEvadeMode();
                return;
            }

            //CataclysmicBolt_Timer
            if (CataclysmicBolt_Timer <= diff)
            {
                //select a random unit other than the main tank
                Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 1);

                //if there aren't other units, cast on the tank
                if (!target)
                    target = me->getVictim();

                if (target)
                    DoCast(target, SPELL_CATACLYSMIC_BOLT);
                CataclysmicBolt_Timer = 10000;
            } else CataclysmicBolt_Timer -= diff;

            //SearNova_Timer
            if (SearNova_Timer <= diff)
            {
                DoCast(me->getVictim(), SPELL_SEAR_NOVA);
                SearNova_Timer = 20000+rand()%40000;
            } else SearNova_Timer -= diff;

            //Enrage_Timer
            if (Enrage_Timer <= diff)
            {
                DoCast(me, SPELL_ENRAGE);
                Enrage_Timer = 90000;
            } else Enrage_Timer -= diff;

            //Blessing of Tides Trigger
            if (!HealthAbovePct(75) && !BlessingOfTides)
            {
                BlessingOfTides = true;
                bool continueTriggering = false;
                Creature* Advisor;
                for (uint8 i = 0; i < MAX_ADVISORS; ++i)
                    if (Advisors[i])
                    {
                        Advisor = (Unit::GetCreature(*me, Advisors[i]));
                        if (Advisor && Advisor->IsAlive())
                        {
                            continueTriggering = true;
                            break;
                        }
                    }
                if (continueTriggering)
                {
                    DoCast(me, SPELL_BLESSING_OF_THE_TIDES);
                    me->MonsterYell(SAY_GAIN_BLESSING_OF_TIDES, LANG_UNIVERSAL, ObjectGuid::Empty);
                    DoPlaySoundToSet(me, SOUND_GAIN_BLESSING_OF_TIDES);
                }
            }

            DoMeleeAttackIfReady();
        }
    };

};

//Fathom-Guard Sharkkis AI
class boss_fathomguard_sharkkis : public CreatureScript
{
public:
    boss_fathomguard_sharkkis() : CreatureScript("boss_fathomguard_sharkkis") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_fathomguard_sharkkisAI (creature);
    }

    struct boss_fathomguard_sharkkisAI : public ScriptedAI
    {
        boss_fathomguard_sharkkisAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;

        uint32 LeechingThrow_Timer;
        uint32 TheBeastWithin_Timer;
        uint32 Multishot_Timer;
        uint32 Pet_Timer;

        bool pet;

        ObjectGuid SummonedPet;

        void Reset()
        {
            LeechingThrow_Timer = 20000;
            TheBeastWithin_Timer = 30000;
            Multishot_Timer = 15000;
            Pet_Timer = 10000;

            pet = false;

            Creature* Pet = Unit::GetCreature(*me, SummonedPet);
            if (Pet && Pet->IsAlive())
            {
                Pet->DealDamage(Pet, Pet->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            }

            SummonedPet.Clear();

            if (instance)
                instance->SetData(DATA_KARATHRESSEVENT, NOT_STARTED);
        }

        void JustDied(Unit* /*killer*/)
        {
            if (instance)
            {
                Creature* Karathress = NULL;
                Karathress = (Unit::GetCreature((*me), instance->GetGuidData(DATA_KARATHRESS)));

                if (Karathress)
                    if (!me->IsAlive() && Karathress)
                        CAST_AI(boss_fathomlord_karathress::boss_fathomlord_karathressAI, Karathress->AI())->EventSharkkisDeath();
            }
        }

        void EnterCombat(Unit* who)
        {
            if (instance)
            {
                instance->SetGuidData(DATA_KARATHRESSEVENT_STARTER, who->GetGUID());
                instance->SetData(DATA_KARATHRESSEVENT, IN_PROGRESS);
            }
        }

        void UpdateAI(uint32 diff)
        {
            //Only if not incombat check if the event is started
            if (!me->isInCombat() && instance && instance->GetData(DATA_KARATHRESSEVENT))
            {
                Unit* target = Unit::GetUnit(*me, instance->GetGuidData(DATA_KARATHRESSEVENT_STARTER));

                if (target)
                {
                    AttackStart(target);
                }
            }

            //Return since we have no target
            if (!UpdateVictim())
                return;

            //someone evaded!
            if (instance && !instance->GetData(DATA_KARATHRESSEVENT))
            {
                EnterEvadeMode();
                return;
            }

            //LeechingThrow_Timer
            if (LeechingThrow_Timer <= diff)
            {
                DoCast(me->getVictim(), SPELL_LEECHING_THROW);
                LeechingThrow_Timer = 20000;
            } else LeechingThrow_Timer -= diff;

            //Multishot_Timer
            if (Multishot_Timer <= diff)
            {
                DoCast(me->getVictim(), SPELL_MULTISHOT);
                Multishot_Timer = 20000;
            } else Multishot_Timer -= diff;

            //TheBeastWithin_Timer
            if (TheBeastWithin_Timer <= diff)
            {
                DoCast(me, SPELL_THE_BEAST_WITHIN);
                Creature* Pet = Unit::GetCreature(*me, SummonedPet);
                if (Pet && Pet->IsAlive())
                {
                    Pet->CastSpell(Pet, SPELL_PET_ENRAGE, true);
                }
                TheBeastWithin_Timer = 30000;
            } else TheBeastWithin_Timer -= diff;

            //Pet_Timer
            if (Pet_Timer < diff && pet == false)
            {
                pet = true;
                //uint32 spell_id;
                uint32 pet_id;
                if (!urand(0, 1))
                {
                    //spell_id = SPELL_SUMMON_FATHOM_LURKER;
                    pet_id = CREATURE_FATHOM_LURKER;
                }
                else
                {
                    //spell_id = SPELL_SUMMON_FATHOM_SPOREBAT;
                    pet_id = CREATURE_FATHOM_SPOREBAT;
                }
                //DoCast(me, spell_id, true);
                Creature* Pet = DoSpawnCreature(pet_id, 0, 0, 0, 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15000);
                Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0);
                if (Pet && target)
                {
                    Pet->AI()->AttackStart(target);
                    SummonedPet = Pet->GetGUID();
                }
            } else Pet_Timer -= diff;

            DoMeleeAttackIfReady();
        }
    };

};

//Fathom-Guard Tidalvess AI
class boss_fathomguard_tidalvess : public CreatureScript
{
public:
    boss_fathomguard_tidalvess() : CreatureScript("boss_fathomguard_tidalvess") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_fathomguard_tidalvessAI (creature);
    }

    struct boss_fathomguard_tidalvessAI : public ScriptedAI
    {
        boss_fathomguard_tidalvessAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;

        uint32 FrostShock_Timer;
        uint32 Spitfire_Timer;
        uint32 PoisonCleansing_Timer;
        uint32 Earthbind_Timer;

        void Reset()
        {
            FrostShock_Timer = 25000;
            Spitfire_Timer = 60000;
            PoisonCleansing_Timer = 30000;
            Earthbind_Timer = 45000;

            if (instance)
                instance->SetData(DATA_KARATHRESSEVENT, NOT_STARTED);
        }

        void JustDied(Unit* /*killer*/)
        {
            if (instance)
            {
                Creature* Karathress = NULL;
                Karathress = (Unit::GetCreature((*me), instance->GetGuidData(DATA_KARATHRESS)));

                if (Karathress)
                    if (!me->IsAlive() && Karathress)
                        CAST_AI(boss_fathomlord_karathress::boss_fathomlord_karathressAI, Karathress->AI())->EventTidalvessDeath();
            }
        }

        void EnterCombat(Unit* who)
        {
            if (instance)
            {
                instance->SetGuidData(DATA_KARATHRESSEVENT_STARTER, who->GetGUID());
                instance->SetData(DATA_KARATHRESSEVENT, IN_PROGRESS);
            }
            DoCast(me, SPELL_WINDFURY_WEAPON);
        }

        void UpdateAI(uint32 diff)
        {
            //Only if not incombat check if the event is started
            if (!me->isInCombat() && instance && instance->GetData(DATA_KARATHRESSEVENT))
            {
                Unit* target = Unit::GetUnit(*me, instance->GetGuidData(DATA_KARATHRESSEVENT_STARTER));

                if (target)
                {
                    AttackStart(target);
                }
            }

            //Return since we have no target
            if (!UpdateVictim())
                return;

            //someone evaded!
            if (instance && !instance->GetData(DATA_KARATHRESSEVENT))
            {
                EnterEvadeMode();
                return;
            }

            if (!me->HasAura(SPELL_WINDFURY_WEAPON))
            {
                DoCast(me, SPELL_WINDFURY_WEAPON);
            }

            //FrostShock_Timer
            if (FrostShock_Timer <= diff)
            {
                DoCast(me->getVictim(), SPELL_FROST_SHOCK);
                FrostShock_Timer = 25000+rand()%5000;
            } else FrostShock_Timer -= diff;

            //Spitfire_Timer
            if (Spitfire_Timer <= diff)
            {
                DoCast(me, SPELL_SPITFIRE_TOTEM);
                //ToDo: CREATURE_SPITFIRE_TOTEM is not guid get from instance
                //Unit* SpitfireTotem = Unit::GetUnit(*me, CREATURE_SPITFIRE_TOTEM);
                //if (SpitfireTotem)
                //{
                //    CAST_CRE(SpitfireTotem)->AI()->AttackStart(me->getVictim());
                //}
                Spitfire_Timer = 60000;
            } else Spitfire_Timer -= diff;

            //PoisonCleansing_Timer
            if (PoisonCleansing_Timer <= diff)
            {
                DoCast(me, SPELL_POISON_CLEANSING_TOTEM);
                PoisonCleansing_Timer = 30000;
            } else PoisonCleansing_Timer -= diff;

            //Earthbind_Timer
            if (Earthbind_Timer <= diff)
            {
                DoCast(me, SPELL_EARTHBIND_TOTEM);
                Earthbind_Timer = 45000;
            } else Earthbind_Timer -= diff;

            DoMeleeAttackIfReady();
        }
    };

};

//Fathom-Guard Caribdis AI
class boss_fathomguard_caribdis : public CreatureScript
{
public:
    boss_fathomguard_caribdis() : CreatureScript("boss_fathomguard_caribdis") { }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_fathomguard_caribdisAI (creature);
    }

    struct boss_fathomguard_caribdisAI : public ScriptedAI
    {
        boss_fathomguard_caribdisAI(Creature* creature) : ScriptedAI(creature)
        {
            instance = creature->GetInstanceScript();
        }

        InstanceScript* instance;

        uint32 WaterBoltVolley_Timer;
        uint32 TidalSurge_Timer;
        uint32 Heal_Timer;
        uint32 Cyclone_Timer;

        void Reset()
        {
            WaterBoltVolley_Timer = 35000;
            TidalSurge_Timer = 15000+rand()%5000;
            Heal_Timer = 55000;
            Cyclone_Timer = 30000+rand()%10000;

            if (instance)
                instance->SetData(DATA_KARATHRESSEVENT, NOT_STARTED);
        }

        void JustDied(Unit* /*killer*/)
        {
            if (instance)
            {
                Creature* Karathress = NULL;
                Karathress = (Unit::GetCreature((*me), instance->GetGuidData(DATA_KARATHRESS)));

                if (Karathress)
                    if (!me->IsAlive() && Karathress)
                        CAST_AI(boss_fathomlord_karathress::boss_fathomlord_karathressAI, Karathress->AI())->EventCaribdisDeath();
            }
        }

        void EnterCombat(Unit* who)
        {
            if (instance)
            {
                instance->SetGuidData(DATA_KARATHRESSEVENT_STARTER, who->GetGUID());
                instance->SetData(DATA_KARATHRESSEVENT, IN_PROGRESS);
            }
        }

        void UpdateAI(uint32 diff)
        {
            //Only if not incombat check if the event is started
            if (!me->isInCombat() && instance && instance->GetData(DATA_KARATHRESSEVENT))
            {
                Unit* target = Unit::GetUnit(*me, instance->GetGuidData(DATA_KARATHRESSEVENT_STARTER));

                if (target)
                {
                    AttackStart(target);
                }
            }

            //Return since we have no target
            if (!UpdateVictim())
                return;

            //someone evaded!
            if (instance && !instance->GetData(DATA_KARATHRESSEVENT))
            {
                EnterEvadeMode();
                return;
            }

            //WaterBoltVolley_Timer
            if (WaterBoltVolley_Timer <= diff)
            {
                DoCast(me->getVictim(), SPELL_WATER_BOLT_VOLLEY);
                WaterBoltVolley_Timer = 30000;
            } else WaterBoltVolley_Timer -= diff;

            //TidalSurge_Timer
            if (TidalSurge_Timer <= diff)
            {
                if(Unit* victim = me->getVictim())
                {
                    DoCast(victim, SPELL_TIDAL_SURGE);
                    // Hacky way to do it - won't trigger elseways
                    victim->CastSpell(victim, SPELL_TIDAL_SURGE_FREEZE, true);
                }
                TidalSurge_Timer = 15000+rand()%5000;
            } else TidalSurge_Timer -= diff;

            //Cyclone_Timer
            if (Cyclone_Timer <= diff)
            {
                //DoCast(me, SPELL_SUMMON_CYCLONE); // Doesn't work
                Cyclone_Timer = 30000+rand()%10000;
                Creature* Cyclone = me->SummonCreature(CREATURE_CYCLONE, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), float(rand()%5), TEMPSUMMON_TIMED_DESPAWN, 15000);
                if (Cyclone)
                {
                    CAST_CRE(Cyclone)->SetObjectScale(3.0f);
                    Cyclone->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    Cyclone->setFaction(me->getFaction());
                    Cyclone->CastSpell(Cyclone, SPELL_CYCLONE_CYCLONE, true);
                    Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0);
                    if (target)
                    {
                        Cyclone->AI()->AttackStart(target);
                    }
                }
            } else Cyclone_Timer -= diff;

            //Heal_Timer
            if (Heal_Timer <= diff)
            {
                // It can be cast on any of the mobs
                Unit* unit = NULL;

                while (unit == NULL || !unit->IsAlive())
                {
                    unit = selectAdvisorUnit();
                }

                if (unit && unit->IsAlive())
                    DoCast(unit, SPELL_HEAL);
                Heal_Timer = 60000;
            } else Heal_Timer -= diff;

            DoMeleeAttackIfReady();
        }

        Unit* selectAdvisorUnit()
        {
            Unit* unit = NULL;
            if (instance)
            {
                switch (rand()%4)
                {
                case 0:
                    unit = Unit::GetUnit(*me, instance->GetGuidData(DATA_KARATHRESS));
                    break;
                case 1:
                    unit = Unit::GetUnit(*me, instance->GetGuidData(DATA_SHARKKIS));
                    break;
                case 2:
                    unit = Unit::GetUnit(*me, instance->GetGuidData(DATA_TIDALVESS));
                    break;
                case 3:
                    unit = me;
                    break;
                }
            } else unit = me;
            return unit;
        }
    };
};

void AddSC_boss_fathomlord_karathress()
{
    new boss_fathomlord_karathress();
    new boss_fathomguard_sharkkis();
    new boss_fathomguard_tidalvess();
    new boss_fathomguard_caribdis();
}
