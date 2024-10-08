/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2006-2007 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
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
SDName: instance_uldaman
SD%Complete: 99
SDComment: Need some cosmetics updates when archeadas door are closing (Guardians Waypoints).
SDCategory: Uldaman
EndScriptData */

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "uldaman.h"

enum eSpells
{
    SPELL_ARCHAEDAS_AWAKEN      = 10347,
    SPELL_AWAKEN_VAULT_WALKER   = 10258,
};

class instance_uldaman : public InstanceMapScript
{
    public:
        instance_uldaman()
            : InstanceMapScript("instance_uldaman", 70)
        {
        }

        struct instance_uldaman_InstanceMapScript : public InstanceScript
        {
            instance_uldaman_InstanceMapScript(Map* map) : InstanceScript(map)
            {
            }

            void Initialize()
            {
                memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));

                uiArchaedasGUID.Clear();
                uiIronayaGUID.Clear();
                uiWhoWokeuiArchaedasGUID.Clear();

                uiAltarOfTheKeeperTempleDoor.Clear();
                uiArchaedasTempleDoor.Clear();
                uiAncientVaultDoor.Clear();

                uiIronayaSealDoor.Clear();

                uiKeystoneGUID.Clear();

                uiIronayaSealDoorTimer = 27000; //animation time
                bKeystoneCheck = false;
            }

            bool IsEncounterInProgress() const
            {
                for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
                    if (m_auiEncounter[i] == IN_PROGRESS)
                        return true;

                return false;
            }

            ObjectGuid uiArchaedasGUID;
            ObjectGuid uiIronayaGUID;
            ObjectGuid uiWhoWokeuiArchaedasGUID;

            ObjectGuid uiAltarOfTheKeeperTempleDoor;
            ObjectGuid uiArchaedasTempleDoor;
            ObjectGuid uiAncientVaultDoor;
            ObjectGuid uiIronayaSealDoor;

            ObjectGuid uiKeystoneGUID;

            uint32 uiIronayaSealDoorTimer;
            bool bKeystoneCheck;

            GuidVector vStoneKeeper;
            GuidVector vAltarOfTheKeeperCount;
            GuidVector vVaultWalker;
            GuidVector vEarthenGuardian;
            GuidVector vArchaedasWallMinions;    // minions lined up around the wall

            uint32 m_auiEncounter[MAX_ENCOUNTER];
            std::string str_data;

            void OnGameObjectCreate(GameObject* go)
            {
                switch (go->GetEntry())
                {
                    case GO_ALTAR_OF_THE_KEEPER_TEMPLE_DOOR:         // lock the door
                        uiAltarOfTheKeeperTempleDoor = go->GetGUID();

                        if (m_auiEncounter[0] == DONE)
                           HandleGameObject(ObjectGuid::Empty, true, go);
                        break;

                    case GO_ARCHAEDAS_TEMPLE_DOOR:
                        uiArchaedasTempleDoor = go->GetGUID();

                        if (m_auiEncounter[0] == DONE)
                            HandleGameObject(ObjectGuid::Empty, true, go);
                        break;

                    case GO_ANCIENT_VAULT_DOOR:
                        go->SetGoState(GO_STATE_READY);
                        go->SetUInt32Value(GAMEOBJECT_FIELD_FLAGS, 33);
                        uiAncientVaultDoor = go->GetGUID();

                        if (m_auiEncounter[1] == DONE)
                            HandleGameObject(ObjectGuid::Empty, true, go);
                        break;

                    case GO_IRONAYA_SEAL_DOOR:
                        uiIronayaSealDoor = go->GetGUID();

                        if (m_auiEncounter[2] == DONE)
                            HandleGameObject(ObjectGuid::Empty, true, go);
                        break;

                    case GO_KEYSTONE:
                        uiKeystoneGUID = go->GetGUID();

                        if (m_auiEncounter[2] == DONE)
                        {
                            HandleGameObject(ObjectGuid::Empty, true, go);
                            go->SetUInt32Value(GAMEOBJECT_FIELD_FLAGS, GO_FLAG_INTERACT_COND);
                        }
                        break;
                }
            }

            void SetFrozenState(Creature* creature)
            {
                creature->setFaction(35);
                creature->RemoveAllAuras();
                //creature->RemoveFlag (UNIT_FIELD_FLAGS, UNIT_FLAG_ANIMATION_FROZEN);
                creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_REMOVE_CLIENT_CONTROL);
            }

            void SetDoor(ObjectGuid guid, bool open)
            {
                GameObject* go = instance->GetGameObject(guid);
                if (!go)
                    return;

                HandleGameObject(guid, open);
            }

            void BlockGO(ObjectGuid guid)
            {
                GameObject* go = instance->GetGameObject(guid);
                if (!go)
                    return;

                go->SetUInt32Value(GAMEOBJECT_FIELD_FLAGS, GO_FLAG_INTERACT_COND);
            }

            void ActivateStoneKeepers()
            {
                for (GuidVector::const_iterator i = vStoneKeeper.begin(); i != vStoneKeeper.end(); ++i)
                {
                    Creature* target = instance->GetCreature(*i);
                    if (!target || !target->IsAlive() || target->getFaction() == 14)
                        continue;
                    target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_REMOVE_CLIENT_CONTROL);
                    target->setFaction(14);
                    target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    return;        // only want the first one we find
                }
                // if we get this far than all four are dead so open the door
                SetData(DATA_ALTAR_DOORS, DONE);
                SetDoor(uiArchaedasTempleDoor, true); //open next the door too
            }

            void ActivateWallMinions()
            {
                Creature* archaedas = instance->GetCreature(uiArchaedasGUID);
                if (!archaedas)
                    return;

                for (GuidVector::const_iterator i = vArchaedasWallMinions.begin(); i != vArchaedasWallMinions.end(); ++i)
                {
                    Creature* target = instance->GetCreature(*i);
                    if (!target || !target->IsAlive() || target->getFaction() == 14)
                        continue;
                    archaedas->CastSpell(target, SPELL_AWAKEN_VAULT_WALKER, true);
                    target->CastSpell(target, SPELL_ARCHAEDAS_AWAKEN, true);
                    target->RemoveFlag(UNIT_FIELD_FLAGS,UNIT_FLAG_REMOVE_CLIENT_CONTROL);
                    target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    target->setFaction(14);
                    return;        // only want the first one we find
                }
            }

            // used when Archaedas dies.  All active minions must be despawned.
            void DeActivateMinions()
            {
                // first despawn any aggroed wall minions
                for (GuidVector::const_iterator i = vArchaedasWallMinions.begin(); i != vArchaedasWallMinions.end(); ++i)
                {
                    Creature* target = instance->GetCreature(*i);
                    if (!target || target->isDead() || target->getFaction() != 14)
                        continue;
                    target->setDeathState(JUST_DIED);
                    target->RemoveCorpse();
                }

                // Vault Walkers
                for (GuidVector::const_iterator i = vVaultWalker.begin(); i != vVaultWalker.end(); ++i)
                {
                    Creature* target = instance->GetCreature(*i);
                    if (!target || target->isDead() || target->getFaction() != 14)
                        continue;
                    target->setDeathState(JUST_DIED);
                    target->RemoveCorpse();
                }

                // Earthen Guardians
                for (GuidVector::const_iterator i = vEarthenGuardian.begin(); i != vEarthenGuardian.end(); ++i)
                {
                    Creature* target = instance->GetCreature(*i);
                    if (!target || target->isDead() || target->getFaction() != 14)
                        continue;
                    target->setDeathState(JUST_DIED);
                    target->RemoveCorpse();
                }
            }

            void ActivateArchaedas(ObjectGuid target)
            {
                Creature* archaedas = instance->GetCreature(uiArchaedasGUID);
                if (!archaedas)
                    return;

                if (Unit::GetUnit(*archaedas, target))
                {
                    archaedas->CastSpell(archaedas, SPELL_ARCHAEDAS_AWAKEN, false);
                    uiWhoWokeuiArchaedasGUID = target;
                }
            }

            void ActivateIronaya()
            {
                Creature* ironaya = instance->GetCreature(uiIronayaGUID);
                if (!ironaya)
                    return;

                ironaya->setFaction(415);
                ironaya->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_REMOVE_CLIENT_CONTROL);
                ironaya->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            }

            void RespawnMinions()
            {
                // first respawn any aggroed wall minions
                for (GuidVector::const_iterator i = vArchaedasWallMinions.begin(); i != vArchaedasWallMinions.end(); ++i)
                {
                    Creature* target = instance->GetCreature(*i);
                    if (target && target->isDead())
                    {
                        target->Respawn();
                        target->GetMotionMaster()->MoveTargetedHome();
                        SetFrozenState(target);
                    }
                }

                // Vault Walkers
                for (GuidVector::const_iterator i = vVaultWalker.begin(); i != vVaultWalker.end(); ++i)
                {
                    Creature* target = instance->GetCreature(*i);
                    if (target && target->isDead())
                    {
                        target->Respawn();
                        target->GetMotionMaster()->MoveTargetedHome();
                        SetFrozenState(target);
                    }
                }

                // Earthen Guardians
                for (GuidVector::const_iterator i = vEarthenGuardian.begin(); i != vEarthenGuardian.end(); ++i)
                {
                    Creature* target = instance->GetCreature(*i);
                    if (target && target->isDead())
                    {
                        target->Respawn();
                        target->GetMotionMaster()->MoveTargetedHome();
                        SetFrozenState(target);
                    }
                }
            }
            void Update(uint32 diff)
            {
                if (!bKeystoneCheck)
                    return;

                if (uiIronayaSealDoorTimer <= diff)
                {
                    ActivateIronaya();

                    SetDoor(uiIronayaSealDoor, true);
                    BlockGO(uiKeystoneGUID);

                    SetData(DATA_IRONAYA_DOOR, DONE); //save state
                    bKeystoneCheck = false;
                }
                else
                    uiIronayaSealDoorTimer -= diff;
            }

            void SetData(uint32 type, uint32 data)
            {
                switch (type)
                {
                    case DATA_ALTAR_DOORS:
                        m_auiEncounter[0] = data;
                        if (data == DONE)
                            SetDoor(uiAltarOfTheKeeperTempleDoor, true);
                        break;

                    case DATA_ANCIENT_DOOR:
                        m_auiEncounter[1] = data;
                        if (data == DONE) //archeadas defeat
                        {
                            SetDoor(uiArchaedasTempleDoor, true); //re open enter door
                            SetDoor(uiAncientVaultDoor, true);
                        }
                        break;

                    case DATA_IRONAYA_DOOR:
                        m_auiEncounter[2] = data;
                        break;

                    case DATA_STONE_KEEPERS:
                        ActivateStoneKeepers();
                        break;

                    case DATA_MINIONS:
                        switch (data)
                        {
                            case NOT_STARTED:
                                if (m_auiEncounter[0] == DONE) //if players opened the doors
                                    SetDoor(uiArchaedasTempleDoor, true);

                                RespawnMinions();
                                break;

                            case IN_PROGRESS:
                                ActivateWallMinions();
                                break;

                            case SPECIAL:
                                DeActivateMinions();
                                break;
                        }
                        break;

                    case DATA_IRONAYA_SEAL:
                        bKeystoneCheck = true;
                        break;
                }

                if (data == DONE)
                {
                    OUT_SAVE_INST_DATA;

                    std::ostringstream saveStream;
                    saveStream << m_auiEncounter[0] << ' ' << m_auiEncounter[1] << ' ' << m_auiEncounter[2];

                    str_data = saveStream.str();

                    SaveToDB();
                    OUT_SAVE_INST_DATA_COMPLETE;
                }
            }

            void SetGuidData(uint32 type, ObjectGuid data)
            {
                // Archaedas
                if (type == 0)
                {
                    ActivateArchaedas (data);
                    SetDoor(uiArchaedasTempleDoor, false); //close when event is started
                }
            }

            std::string GetSaveData()
            {
                return str_data;
            }

            void Load(const char* in)
            {
                if (!in)
                {
                    OUT_LOAD_INST_DATA_FAIL;
                    return;
                }

                OUT_LOAD_INST_DATA(in);

                std::istringstream loadStream(in);
                loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2];

                for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
                {
                    if (m_auiEncounter[i] == IN_PROGRESS)
                        m_auiEncounter[i] = NOT_STARTED;
                }

                OUT_LOAD_INST_DATA_COMPLETE;
            }

            void OnCreatureCreate(Creature* creature)
            {
                switch (creature->GetEntry())
                {
                    case 4857:    // Stone Keeper
                        SetFrozenState (creature);
                        vStoneKeeper.push_back(creature->GetGUID());
                        break;

                    case 7309:    // Earthen Custodian
                        vArchaedasWallMinions.push_back(creature->GetGUID());
                        break;

                    case 7077:    // Earthen Hallshaper
                        vArchaedasWallMinions.push_back(creature->GetGUID());
                        break;

                    case 7076:    // Earthen Guardian
                        vEarthenGuardian.push_back(creature->GetGUID());
                        break;

                    case 7228:    // Ironaya
                        uiIronayaGUID = creature->GetGUID();

                        if (m_auiEncounter[2] != DONE)
                            SetFrozenState (creature);
                        break;

                    case 10120:    // Vault Walker
                        vVaultWalker.push_back(creature->GetGUID());
                        break;

                    case 2748:    // Archaedas
                        uiArchaedasGUID = creature->GetGUID();
                        break;

                }
            }

            ObjectGuid GetGuidData(uint32 identifier) const
            {
                if (identifier == 0) return uiWhoWokeuiArchaedasGUID;
                if (identifier == 1) return vVaultWalker[0];    // VaultWalker1
                if (identifier == 2) return vVaultWalker[1];    // VaultWalker2
                if (identifier == 3) return vVaultWalker[2];    // VaultWalker3
                if (identifier == 4) return vVaultWalker[3];    // VaultWalker4

                if (identifier == 5) return vEarthenGuardian[0];
                if (identifier == 6) return vEarthenGuardian[1];
                if (identifier == 7) return vEarthenGuardian[2];
                if (identifier == 8) return vEarthenGuardian[3];
                if (identifier == 9) return vEarthenGuardian[4];
                if (identifier == 10) return vEarthenGuardian[5];

                return ObjectGuid::Empty;
            } // end GetGuidData
        };

        InstanceScript* GetInstanceScript(InstanceMap* map) const
        {
            return new instance_uldaman_InstanceMapScript(map);
        }
};

void AddSC_instance_uldaman()
{
    new instance_uldaman();
}
