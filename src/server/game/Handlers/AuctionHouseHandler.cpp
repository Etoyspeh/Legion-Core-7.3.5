/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
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

#include "AuctionHousePackets.h"
#include "AuctionHouseMgr.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"

void WorldSession::HandleAuctionHelloRequest(WorldPackets::AuctionHouse::AuctionHelloRequest& packet)
{
    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(packet.Guid, UNIT_NPC_FLAG_AUCTIONEER);
    if (!unit)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAuctionHelloRequest - Unit (GUID: %u) not found or you can't interact with him.", uint32(packet.Guid.GetGUIDLow()));
        return;
    }

    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    SendAuctionHello(packet.Guid, unit);
}

void WorldSession::SendAuctionHello(ObjectGuid guid, Creature* unit)
{
    if (GetPlayer()->getLevel() < sWorld->getIntConfig(CONFIG_AUCTION_LEVEL_REQ))
    {
        SendNotification(GetTrinityString(LANG_AUCTION_REQ), sWorld->getIntConfig(CONFIG_AUCTION_LEVEL_REQ));
        return;
    }

    AuctionHouseEntry const* ahEntry = AuctionHouseMgr::GetAuctionHouseEntry(unit->getFaction());
    if (!ahEntry)
        return;

    WorldPackets::AuctionHouse::AuctionHelloResponse auctionHelloResponse;
    auctionHelloResponse.Guid = guid;
    auctionHelloResponse.OpenForBusiness = true;                         // 3.3.3: 1 - AH enabled, 0 - AH disabled
    SendPacket(auctionHelloResponse.Write());
}

void WorldSession::SendAuctionCommandResult(AuctionEntry* auction, uint32 action, uint32 errorCode, uint32 /*bidError = 0*/)
{
    WorldPackets::AuctionHouse::AuctionCommandResult auctionCommandResult;
    auctionCommandResult.InitializeAuction(auction);
    auctionCommandResult.Command = action;
    auctionCommandResult.ErrorCode = errorCode;
    SendPacket(auctionCommandResult.Write());
}

void WorldSession::SendAuctionOutBidNotification(AuctionEntry const* auction, Item const* item)
{
    WorldPackets::AuctionHouse::AuctionOutBidNotification packet;
    packet.BidAmount = auction->bid;
    packet.MinIncrement = auction->GetAuctionOutBid();
    packet.Info.Initialize(auction, item);
    SendPacket(packet.Write());
}

void WorldSession::SendAuctionClosedNotification(AuctionEntry const* auction, float mailDelay, bool sold, Item const* item)
{
    WorldPackets::AuctionHouse::AuctionClosedNotification packet;
    packet.Info.Initialize(auction, item);
    packet.ProceedsMailDelay = mailDelay;
    packet.Sold = sold;
    SendPacket(packet.Write());
}

void WorldSession::SendAuctionWonNotification(AuctionEntry const* auction, Item const* item)
{
    WorldPackets::AuctionHouse::AuctionWonNotification packet;
    packet.Info.Initialize(auction, item);
    SendPacket(packet.Write());
}

void WorldSession::SendAuctionOwnerBidNotification(AuctionEntry const* auction, Item const* item)
{
    WorldPackets::AuctionHouse::AuctionOwnerBidNotification packet;
    packet.Info.Initialize(auction, item);
    packet.Bidder = auction->Bidder;
    packet.MinIncrement = auction->GetAuctionOutBid();
    SendPacket(packet.Write());
}

void WorldSession::HandleAuctionSellItem(WorldPackets::AuctionHouse::AuctionSellItem& packet)
{
    for (auto const& item : packet.Items)
        if (!item.Guid || !item.UseCount || item.UseCount > 1000)
            return;

    if (!packet.MinBid || !packet.RunTime)
        return;

    if (packet.MinBid > MAX_MONEY_AMOUNT || packet.BuyoutPrice > MAX_MONEY_AMOUNT)
    {
        SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_DATABASE_ERROR);
        return;
    }

    Creature* creature = GetPlayer()->GetNPCIfCanInteractWith(packet.Auctioneer, UNIT_NPC_FLAG_AUCTIONEER);
    if (!creature)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAuctionSellItem - Unit (GUID: %u) not found or you can't interact with him.", packet.Auctioneer.GetGUIDLow());
        return;
    }

    AuctionHouseEntry const* auctionHouseEntry = AuctionHouseMgr::GetAuctionHouseEntry(creature->getFaction());
    if (!auctionHouseEntry)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAuctionSellItem - Unit (GUID: %u) has wrong faction.", packet.Auctioneer.GetGUIDLow());
        return;
    }

    packet.RunTime *= MINUTE;

    switch (packet.RunTime)
    {
        case 1 * MIN_AUCTION_TIME:
        case 2 * MIN_AUCTION_TIME:
        case 4 * MIN_AUCTION_TIME:
            break;
        default:
            return;
    }

    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    uint32 finalCount = 0;

    for (auto const& packetItem : packet.Items)
    {
        Item* item = _player->GetItemByGuid(packetItem.Guid);

        if (!item)
        {
            SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_ITEM_NOT_FOUND);
            return;
        }

        if (sAuctionMgr->GetAItem(item->GetGUIDLow()) || !item->CanBeTraded() || item->IsNotEmptyBag() ||
            item->GetTemplate()->GetFlags() & ITEM_FLAG_CONJURED || item->GetUInt32Value(ITEM_FIELD_EXPIRATION) ||
            item->GetCount() < packetItem.UseCount || _player->GetItemCount(item->GetEntry(), false) < packetItem.UseCount)
        {
            SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_DATABASE_ERROR);
            return;
        }

        if (packet.Items.empty() && (packetItem.Guid.GetEntry() != item->GetEntry() || packetItem.Guid.GetGUIDLow() == item->GetGUIDLow()))
        {
            sWorld->BanAccount(BAN_CHARACTER, _player->GetName(), "45d", "Dupe Auction mop", "System");
            return;
        }

        finalCount += packetItem.UseCount;
    }

    if (packet.Items.empty())
    {
        SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_DATABASE_ERROR);
        return;
    }

    if (!finalCount)
    {
        SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_DATABASE_ERROR);
        return;
    }

    for (size_t i = 0; i < packet.Items.size() - 1; ++i)
    {
        for (size_t j = i + 1; j < packet.Items.size(); ++j)
        {
            if (packet.Items[i].Guid == packet.Items[j].Guid)
            {
                SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_DATABASE_ERROR);
                return;
            }
        }
    }

    for (auto const& packetItem : packet.Items)
    {
        Item* item = _player->GetItemByGuid(packetItem.Guid);

        if (item->GetMaxStackCount() < finalCount)
        {
            SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_DATABASE_ERROR);
            return;
        }
    }

    for (auto const& packetItem : packet.Items)
    {
        Item* item = _player->GetItemByGuid(packetItem.Guid);

        auto auctionTime = uint32(packet.RunTime * sWorld->getRate(RATE_AUCTION_TIME));
        AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(creature->getFaction());

        uint32 deposit = sAuctionMgr->GetAuctionDeposit(auctionHouseEntry, packet.RunTime, item, finalCount);
        if (!_player->HasEnoughMoney(static_cast<uint64>(deposit)))
        {
            SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_NOT_ENOUGHT_MONEY);
            return;
        }

        _player->ModifyMoney(-int32(deposit));

        auto AH = new AuctionEntry;
        AH->Id = sObjectMgr->GenerateAuctionID();

        if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
            AH->houseId = AUCTIONHOUSE_NEUTRAL;
        else
        {
            CreatureData const* auctioneerData = sObjectMgr->GetCreatureData(creature->GetDBTableGUIDLow());
            if (!auctioneerData)
            {
                TC_LOG_ERROR("misc", "Data for auctioneer not found (%s)", packet.Auctioneer.ToString().c_str());
                SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_DATABASE_ERROR);
                delete AH;
                return;
            }

            CreatureTemplate const* auctioneerInfo = sObjectMgr->GetCreatureTemplate(auctioneerData->id);
            if (!auctioneerInfo)
            {
                TC_LOG_ERROR("misc", "Non existing auctioneer (%s)", packet.Auctioneer.ToString().c_str());
                SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_DATABASE_ERROR);
                delete AH;
                return;
            }

            AuctionHouseEntry const* AHEntry = sAuctionMgr->GetAuctionHouseEntry(auctioneerInfo->faction);
            AH->houseId = AHEntry->ID;
        }

        // Required stack size of auction matches to current item stack size, just move item to auctionhouse
        if (packet.Items.size() == 1 && item->GetCount() == packet.Items[0].UseCount)
        {
            if (GetSecurity() > SEC_PLAYER && sWorld->getBoolConfig(CONFIG_GM_LOG_TRADE))
            {
                sLog->outCommand(GetAccountId(), "GM %s (Account: %u) create auction: %s (Entry: %u Count: %u)",
                    GetPlayerName().c_str(), GetAccountId(), item->GetTemplate()->GetName()->Str[_player->GetSession()->GetSessionDbLocaleIndex()], item->GetEntry(), item->GetCount());
            }

            AH->itemGUIDLow = item->GetGUIDLow();
            AH->itemEntry = item->GetEntry();
            AH->itemCount = item->GetCount();
            AH->Owner = _player->GetGUID();
            AH->startbid = packet.MinBid;
            AH->Bidder = ObjectGuid::Empty;
            AH->bid = 0;
            AH->buyout = packet.BuyoutPrice;
            AH->expire_time = GameTime::GetGameTime() + auctionTime;
            AH->deposit = deposit;
            AH->auctionHouseEntry = auctionHouseEntry;

            sAuctionMgr->AddAItem(item);
            auctionHouse->AddAuction(AH);

            _player->MoveItemFromInventory(item, true);

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            item->DeleteFromInventoryDB(trans);
            item->SaveToDB(trans);
            AH->SaveToDB(trans);
            _player->SaveInventoryAndGoldToDB(trans);
            CharacterDatabase.CommitTransaction(trans);

            SendAuctionCommandResult(AH, AUCTION_SELL_ITEM, ERR_AUCTION_OK);

            GetPlayer()->UpdateAchievementCriteria(CRITERIA_TYPE_CREATE_AUCTION, 1);
            return;
        }
            // Required stack size of auction does not match to current item stack size, clone item and set correct stack size
        Item* newItem = item->CloneItem(finalCount, _player);
        if (!newItem)
        {
            SendAuctionCommandResult(nullptr, AUCTION_SELL_ITEM, ERR_AUCTION_DATABASE_ERROR);
            return;
        }

        //if (newItem->GetEntry() == 38186)
        //    TC_LOG_DEBUG(LOG_FILTER_EFIR, "HandleAuctionSellItem - CloneItem of item %u; finalCount = %u playerGUID %u, itemGUID %u", newItem->GetEntry(), finalCount, _player->GetGUID().GetCounter(), newItem->GetGUID().GetCounter());

        if (GetSecurity() > SEC_PLAYER && sWorld->getBoolConfig(CONFIG_GM_LOG_TRADE))
        {
            sLog->outCommand(GetAccountId(), "GM %s (Account: %u) create auction: %s (Entry: %u Count: %u)",
            GetPlayerName().c_str(), GetAccountId(), newItem->GetTemplate()->GetName()->Str[_player->GetSession()->GetSessionDbLocaleIndex()], newItem->GetEntry(), newItem->GetCount());
        }

        AH->itemGUIDLow = newItem->GetGUIDLow();
        AH->itemEntry = newItem->GetEntry();
        AH->itemCount = newItem->GetCount();
        AH->Owner = _player->GetGUID();
        AH->startbid = packet.MinBid;
        AH->Bidder = ObjectGuid::Empty;
        AH->bid = 0;
        AH->buyout = packet.BuyoutPrice;
        AH->expire_time = GameTime::GetGameTime() + auctionTime;
        AH->deposit = deposit;
        AH->auctionHouseEntry = auctionHouseEntry;

        sAuctionMgr->AddAItem(newItem);
        auctionHouse->AddAuction(AH);

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        for (auto const& v : packet.Items)
        {
            Item* item2 = _player->GetItemByGuid(v.Guid);

            // Item stack count equals required count, ready to delete item - cloned item will be used for auction
            if (item2->GetCount() == v.UseCount)
            {
                _player->MoveItemFromInventory(item2, true);
                item2->DeleteFromInventoryDB(trans);
                item2->DeleteFromDB(trans);
                delete item2;
            }
            else // Item stack count is bigger than required count, update item stack count and save to database - cloned item will be used for auction
            {
                item2->SetCount(item2->GetCount() - v.UseCount);
                item2->SetState(ITEM_CHANGED, _player);
                _player->ItemRemovedQuestCheck(item2->GetEntry(), v.UseCount);
                item2->SendUpdateToPlayer(_player);
                item2->SaveToDB(trans);
            }
        }

        newItem->SaveToDB(trans);
        AH->SaveToDB(trans);
        _player->SaveInventoryAndGoldToDB(trans);
        CharacterDatabase.CommitTransaction(trans);

        SendAuctionCommandResult(AH, AUCTION_SELL_ITEM, ERR_AUCTION_OK);

        GetPlayer()->UpdateAchievementCriteria(CRITERIA_TYPE_CREATE_AUCTION, 1);
        return;
    }
}

void WorldSession::HandleAuctionPlaceBid(WorldPackets::AuctionHouse::AuctionPlaceBid& packet)
{
    if (!packet.AuctionItemID || !packet.BidAmount)
        return;    
    // check for cheaters
    Player* player = GetPlayer();

    Creature* creature = player->GetNPCIfCanInteractWith(packet.Auctioneer, UNIT_NPC_FLAG_AUCTIONEER);
    if (!creature)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAuctionPlaceBid - Unit (GUID: %u) not found or you can't interact with him.", uint32(packet.Auctioneer.GetGUIDLow()));
        return;
    }

    if (player->HasUnitState(UNIT_STATE_DIED))
        player->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(creature->getFaction());
    AuctionEntry* auction = auctionHouse->GetAuction(packet.AuctionItemID);

    if (!auction || auction->Owner == player->GetGUID())
    {
        //you cannot bid your own auction:
        SendAuctionCommandResult(nullptr, AUCTION_PLACE_BID, ERR_AUCTION_BID_OWN);
        return;
    }

    // impossible have online own another character (use this for speedup check in case online owner)
    /*Player* auction_owner = ObjectAccessor::FindPlayer(MAKE_NEW_GUID(auction->owner, 0, HighGuid::Player));
    if (!auction_owner && ObjectMgr::GetPlayerAccountIdByGUID(MAKE_NEW_GUID(auction->owner, 0, HighGuid::Player)) == player->GetSession()->GetAccountId())
    {
        //you cannot bid your another character auction:
        SendAuctionCommandResult(NULL, AUCTION_PLACE_BID, ERR_AUCTION_BID_OWN);
        return;
    }*/

    // cheating
    if (packet.BidAmount <= auction->bid || packet.BidAmount < auction->startbid)
        return;

    // BidAmount too low for next bid if not buyout
    if ((packet.BidAmount < auction->buyout || auction->buyout == 0) &&
        packet.BidAmount < auction->bid + auction->GetAuctionOutBid())
    {
        // client already test it but just in case ...
        SendAuctionCommandResult(auction, AUCTION_PLACE_BID, ERR_AUCTION_HIGHER_BID);
        return;
    }

    if (!player->HasEnoughMoney(packet.BidAmount))
    {
        // client already test it but just in case ...
        SendAuctionCommandResult(auction, AUCTION_PLACE_BID, ERR_AUCTION_NOT_ENOUGHT_MONEY);
        return;
    }

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    if (packet.BidAmount < auction->buyout || auction->buyout == 0)
    {
        if (!auction->Bidder.IsEmpty())
        {
            if (auction->Bidder == player->GetGUID())
                player->ModifyMoney(-int64(packet.BidAmount - auction->bid));
            else
            {
                // mail to last bidder and return money
                sAuctionMgr->SendAuctionOutbiddedMail(auction, packet.BidAmount, player, trans);
                player->ModifyMoney(-int64(packet.BidAmount));
            }
        }
        else
            player->ModifyMoney(-int64(packet.BidAmount));

        auction->Bidder = player->GetGUID();
        auction->bid = packet.BidAmount;
        player->UpdateAchievementCriteria(CRITERIA_TYPE_HIGHEST_AUCTION_BID, packet.BidAmount);

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_AUCTION_BID);
        stmt->setUInt32(0, auction->Bidder.GetCounter());
        stmt->setUInt32(1, auction->bid);
        stmt->setUInt32(2, auction->Id);
        trans->Append(stmt);

        SendAuctionCommandResult(auction, AUCTION_PLACE_BID, ERR_AUCTION_OK);
    }
    else
    {
        //buyout:
        if (player->GetGUID() == auction->Bidder)
            player->ModifyMoney(-int64(auction->buyout - auction->bid));
        else
        {
            player->ModifyMoney(-int64(auction->buyout));
            if (!auction->Bidder.IsEmpty())                          //buyout for bidded auction ..
                sAuctionMgr->SendAuctionOutbiddedMail(auction, auction->buyout, player, trans);
        }
        auction->Bidder = player->GetGUID();
        auction->bid = auction->buyout;
        player->UpdateAchievementCriteria(CRITERIA_TYPE_HIGHEST_AUCTION_BID, auction->buyout);

        SendAuctionCommandResult(auction, AUCTION_PLACE_BID, ERR_AUCTION_OK);

        //- Mails must be under transaction control too to prevent data loss
        sAuctionMgr->SendAuctionSalePendingMail(auction, trans);
        sAuctionMgr->SendAuctionSuccessfulMail(auction, trans);
        sAuctionMgr->SendAuctionWonMail(auction, trans);

        auction->DeleteFromDB(trans);

        uint32 itemEntry = auction->itemEntry;
        sAuctionMgr->RemoveAItem(auction->itemGUIDLow);
        auctionHouse->RemoveAuction(auction, itemEntry);
    }
    player->SaveInventoryAndGoldToDB(trans);
    CharacterDatabase.CommitTransaction(trans);
}

void WorldSession::HandleAuctionRemoveItem(WorldPackets::AuctionHouse::AuctionRemoveItem& packet)
{
    Player* player = GetPlayer();

    Creature* creature = player->GetNPCIfCanInteractWith(packet.Auctioneer, UNIT_NPC_FLAG_AUCTIONEER);
    if (!creature)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAuctionRemoveItem - Unit (GUID: %u) not found or you can't interact with him.", uint32(packet.Auctioneer.GetGUIDLow()));
        return;
    }

    if (player->HasUnitState(UNIT_STATE_DIED))
        player->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(creature->getFaction());
    AuctionEntry* auction = auctionHouse->GetAuction(packet.AuctionItemID);

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (auction && auction->Owner == player->GetGUID())
    {
        Item* pItem = sAuctionMgr->GetAItem(auction->itemGUIDLow);
        if (pItem)
        {
            if (!auction->Bidder.IsEmpty())                        // If we have a bidder, we have to send him the money he paid
            {
                uint32 auctionCut = auction->GetAuctionCut();
                if (!player->HasEnoughMoney(static_cast<uint64>(auctionCut)))          //player doesn't have enough money, maybe message needed
                    return;
                sAuctionMgr->SendAuctionCancelledToBidderMail(auction, trans, pItem);
                player->ModifyMoney(-int64(auctionCut));
            }

            // item will deleted or added to received mail list
            MailDraft(auction->BuildAuctionMailSubject(AUCTION_CANCELED), AuctionEntry::BuildAuctionMailBody(0, 0, auction->buyout, auction->deposit, 0))
                .AddItem(pItem)
                .SendMailTo(trans, player, auction, MAIL_CHECK_MASK_COPIED);
        }
        else
        {
            TC_LOG_ERROR("network", "Auction id: %u got non existing item (item guid : %lu)!", auction->Id, auction->itemGUIDLow);
            SendAuctionCommandResult(nullptr, AUCTION_CANCEL, ERR_AUCTION_DATABASE_ERROR);
            return;
        }
    }
    else
    {
        SendAuctionCommandResult(nullptr, AUCTION_CANCEL, ERR_AUCTION_DATABASE_ERROR);
        //this code isn't possible ... maybe there should be assert
        TC_LOG_ERROR("network", "CHEATER: %u tried to cancel auction (id: %u) of another player or auction is NULL", player->GetGUIDLow(), packet.AuctionItemID);
        return;
    }

    //inform player, that auction is removed
    SendAuctionCommandResult(auction, AUCTION_CANCEL, ERR_AUCTION_OK);

    // Now remove the auction

    player->SaveInventoryAndGoldToDB(trans);
    auction->DeleteFromDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    uint32 itemEntry = auction->itemEntry;
    sAuctionMgr->RemoveAItem(auction->itemGUIDLow);
    auctionHouse->RemoveAuction(auction, itemEntry);
}

void WorldSession::HandleAuctionListBidderItems(WorldPackets::AuctionHouse::AuctionListBidderItems& packet)
{
    Player* player = GetPlayer();

    Creature* creature = player->GetNPCIfCanInteractWith(packet.Auctioneer, UNIT_NPC_FLAG_AUCTIONEER);
    if (!creature)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAuctionListBidderItems - Unit (GUID: %u) not found or you can't interact with him.", uint32(packet.Auctioneer.GetGUIDLow()));
        return;
    }

    if (player->HasUnitState(UNIT_STATE_DIED))
        player->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(creature->getFaction());

    WorldPackets::AuctionHouse::AuctionListBidderItemsResult result;
    auctionHouse->BuildListBidderItems(result, player, result.TotalCount);
    result.DesiredDelay = 300;
    SendPacket(result.Write());
}

void WorldSession::HandleAuctionListOwnerItems(WorldPackets::AuctionHouse::AuctionListOwnerItems& packet)
{
    Player* player = GetPlayer();

    Creature* creature = player->GetNPCIfCanInteractWith(packet.Auctioneer, UNIT_NPC_FLAG_AUCTIONEER);
    if (!creature)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAuctionListOwnerItems - Unit (GUID: %u) not found or you can't interact with him.", uint32(packet.Auctioneer.GetGUIDLow()));
        return;
    }

    if (player->HasUnitState(UNIT_STATE_DIED))
        player->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(creature->getFaction());

    WorldPackets::AuctionHouse::AuctionListOwnerItemsResult result;
    auctionHouse->BuildListOwnerItems(result, player, result.TotalCount);
    result.DesiredDelay = 300;
    SendPacket(result.Write());
}

void WorldSession::HandleAuctionListItems(WorldPackets::AuctionHouse::AuctionListItems& packet)
{
    Player* player = GetPlayer();

    Creature* creature = player->GetNPCIfCanInteractWith(packet.Auctioneer, UNIT_NPC_FLAG_AUCTIONEER);
    if (!creature)
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAuctionListItems - Unit (GUID: %u) not found or you can't interact with him.", uint32(packet.Auctioneer.GetGUIDLow()));
        return;
    }

    if (player->HasUnitState(UNIT_STATE_DIED))
        player->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    std::wstring wsearchedname;
    if (!Utf8toWStr(packet.Name, wsearchedname))
        return;

    wstrToLower(wsearchedname);

    Optional<AuctionSearchFilters> filters;

    WorldPackets::AuctionHouse::AuctionListItemsResult result;
    if (!packet.ClassFilters.empty())
    {
        filters.emplace();

        for (auto const& classFilter : packet.ClassFilters)
        {
            if (!classFilter.SubClassFilters.empty())
            {
                for (auto const& subClassFilter : classFilter.SubClassFilters)
                {
                    if (classFilter.ItemClass < MAX_ITEM_CLASS)
                    {
                        filters->Classes[classFilter.ItemClass].SubclassMask |= 1 << subClassFilter.ItemSubclass;
                        if (subClassFilter.ItemSubclass < 21)
                            filters->Classes[classFilter.ItemClass].InvTypes[subClassFilter.ItemSubclass] = subClassFilter.InvTypeMask;
                    }
                }
            }
            else
                filters->Classes[classFilter.ItemClass].SubclassMask = AuctionSearchFilters::FILTER_SKIP_SUBCLASS;
        }
    }

    sAuctionMgr->GetAuctionsMap(creature->getFaction())->BuildListAuctionItems(result, _player, wsearchedname, packet.Offset, packet.MinLevel, packet.MaxLevel, packet.OnlyUsable, filters, packet.Quality);

    result.DesiredDelay = 300;
    result.OnlyUsable = packet.OnlyUsable;
    SendPacket(result.Write());
}

void WorldSession::HandleAuctionListPendingSales(WorldPackets::AuctionHouse::AuctionListPendingSales& /*packet*/)
{
    uint32 count = 0;

    WorldPackets::AuctionHouse::AuctionListPendingSalesResult result;
    for (auto itr = _player->GetMailBegin(); itr != _player->GetMailEnd(); ++itr)
    {
        if ((*itr)->state == MAIL_STATE_DELETED || (*itr)->messageType == MAIL_AUCTION)
            continue;

        ++count;
        if (count < 50)
            result.Mails.emplace_back(*itr, _player);

        break;
    }

    result.TotalNumRecords = count;

    SendPacket(result.Write());
}

void WorldSession::HandleReplicateItems(WorldPackets::AuctionHouse::AuctionReplicateItems& packet)
{
    Creature* creature = GetPlayer()->GetNPCIfCanInteractWith(packet.Auctioneer, UNIT_NPC_FLAG_AUCTIONEER);
    if (!creature)
        return;

    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    WorldPackets::AuctionHouse::AuctionReplicateResponse response;
    sAuctionMgr->GetAuctionsMap(creature->getFaction())->BuildReplicate(response, GetPlayer(), packet.ChangeNumberGlobal, packet.ChangeNumberCursor, packet.ChangeNumberTombstone, packet.Count);
    response.DesiredDelay = 300 * 5;
    response.Result = 0;
    SendPacket(response.Write());
}
