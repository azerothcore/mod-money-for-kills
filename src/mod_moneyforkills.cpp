/*

# Money For Kills #

#### A module for AzerothCore (https://github.com/azerothcore/mod-money-for-kills)
#### Originally by [StygianTheBest](https://github.com/StygianTheBest/AzerothCore-Content/tree/master/Modules)
------------------------------------------------------------------------------------------------------------------


### Description ###
------------------------------------------------------------------------------------------------------------------
I created this module to award more money to players on smaller servers that don't have an AH or a lot of activity.
The player can earn a bounty on every creature killed based on the creature's level. The amounts are configurable.
They also earn larger bounties on dungeon and world bosses. To promote PVP, a player can earn a percentage of the
victim's gold on a kill. There are options to allow players in groups to get bounty rewards only if they are in
reward range of the group and an option to only reward the player that got the killing blow.


### Features ###
------------------------------------------------------------------------------------------------------------------
- Pays players bounty money for kills of players and creatures
- Bounty and other amounts can be changed in the config file
- Bounty can be paid to only the player with killing blow or all players
- Bounty can be paid to players that are near or far away from the group
- Dungeon boss kills are announced to the party
- World boss kills are announced to the world
- Player suicides are announced to the world


### Data ###
------------------------------------------------------------------------------------------------------------------
- Type: Server/Player
- Script: MoneyForKills
- Config: Yes
- Enable/Disable Module
- Enable Module Announce
- Enable Killing Blow Only Bounty
- Enable Bounty for Players Outside Reward Area
- Set % of Gold Looted from victim on PVP kill
- Set Bounty Multipliers for each type of kill
- SQL: No


### Version ###
------------------------------------------------------------------------------------------------------------------
- v2022.04.09 - Fixed deprecation warnings
- v2017.08.24 - Release
- v2017.08.31 - Added boss kills
- v2017.09.02 - Added distance check, Fixed group payment
- v2017.09.22 - Added PVPCorpseLoot as a config option
- v2017.10.11 - Fix typo in color code

### Credits ###
------------------------------------------------------------------------------------------------------------------
- [Blizzard Entertainment](http://blizzard.com)
- [TrinityCore](https://github.com/TrinityCore/TrinityCore/blob/3.3.5/THANKS)
- [SunwellCore](http://www.azerothcore.org/pages/sunwell.pl/)
- [AzerothCore](https://github.com/AzerothCore/azerothcore-wotlk/graphs/contributors)
- [AzerothCore Discord](https://discord.gg/gkt4y2x)
- [EMUDevs](https://youtube.com/user/EmuDevs)
- [AC-Web](http://ac-web.org/)
- [ModCraft.io](http://modcraft.io/)
- [OwnedCore](http://ownedcore.com/)
- [OregonCore](https://wiki.oregon-core.net/)
- [Wowhead.com](http://wowhead.com)
- [AoWoW](https://wotlk.evowow.com/)
- [SoulSeekkor](https://github.com/SoulSeekkor)


### License ###
------------------------------------------------------------------------------------------------------------------
- This code and content is released under the [GNU AGPL v3](https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3).

*/

#include "Config.h"
#include "Group.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "Player.h"
#include "Guild.h"
#include "WorldSessionMgr.h"
#include <algorithm>
#include <cmath>

enum KillType
{
	KILLTYPE_LOOT,
	KILLTYPE_PVP,
	KILLTYPE_DUNGEONBOSS,
	KILLTYPE_WORLDBOSS,
	KILLTYPE_MOB,
	KILLTYPE_SUICIDE
};

static constexpr const char* MFKEnable = "MFK.Enable";
static constexpr const char* MFKAnnounce = "MFK.Announce";
static constexpr const char* MFKAnnounceWorldBoss = "MFK.Announce.World.WorldBoss";
static constexpr const char* MFKAnnounceDungeonBoss = "MFK.Announce.Group.DungeonBoss";
static constexpr const char* MFKAnnounceWorldSuicide = "MFK.Announce.World.Suicide";
static constexpr const char* MFKAnnounceGuildSuicide = "MFK.Announce.Guild.Suicide";
static constexpr const char* MFKAnnounceGroupSuicide = "MFK.Announce.Group.Suicide";
static constexpr const char* MFKAnnouncePvP = "MFK.Announce.World.PvP";
static constexpr const char* MFKBountyKillingBlow = "MFK.Bounty.KillingBlowOnly";
static constexpr const char* MFKBountyMoneyForNothing = "MFK.Bounty.MoneyForNothing";
static constexpr const char* MFKPVPCorpseLootPercent = "MFK.PVP.CorpseLootPercent";
static constexpr const char* MFKBountyKillMult = "MFK.Bounty.Kill.Multiplier";
static constexpr const char* MFKPVPKillMult = "MFK.PVP.Kill.Multiplier";
static constexpr const char* MFKBountyKillDBMult = "MFK.Bounty.DungeonBoss.Multiplier";
static constexpr const char* MFKBountyKillWBMult = "MFK.Bounty.WorldBoss.Multiplier";
static constexpr const char* MFKKillerLevelDiffEnable = "MFK.Killer.Level.Diff.Enable";
static constexpr const char* MFKMaxGoldThreshold = "MFK.Max.Gold.Threshold";

class MoneyForKills : public PlayerScript
{
public:
	MoneyForKills() : PlayerScript("MoneyForKills") { }

	// Announce Module
	void OnPlayerLogin(Player *player) {
		if (sConfigMgr->GetOption<bool>(MFKEnable, true))
		{
			if (sConfigMgr->GetOption<bool>(MFKAnnounce, true))
			{
				ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00MoneyForKills |rmodule.");
			}
		}
	}

	// Player Kill Reward
	void OnPlayerPVPKill(Player* killer, Player* victim)
	{
		// If enabled...
		if (sConfigMgr->GetOption<bool>(MFKEnable, true))
		{
			//A coefficient to adjust the gold given for bounties
			const uint32 PVPMultiplier = sConfigMgr->GetOption<uint32>(MFKPVPKillMult, 0);
			const uint32 VictimLevel = victim->GetLevel(); 
			const uint32 KillerLevel = killer->GetLevel();
			//Gold of the victim before calculation
			const uint32 VictimGold = victim->GetMoney();
			//Scales the reward depending on the proportional difference in level if enabled
            float KillerLevelDiff = 1.0f;
			//adding an enable system to choose the multiplier applied
			if (sConfigMgr->GetOption<bool>(MFKKillerLevelDiffEnable, true))
			{
				KillerLevelDiff = static_cast<float>(VictimLevel) / static_cast<float>(KillerLevel);
			}
			//Maximum percentage of victim gold which can be taken. Maximum allowable value is 100%.
			const float MaxLootPercentage = std::min(1.0f,sConfigMgr->GetOption<float>(MFKMaxGoldThreshold, 1.0f));
			// Calculate the percentage of gold to transfer from victim to killer. Maximum alloable is whatever was set in MaxLootPercentage.
			const float PVPCorpseLootPercent = std::min((sConfigMgr->GetOption<uint32>(MFKPVPCorpseLootPercent, 5)/100.0f)*KillerLevelDiff,MaxLootPercentage);
			// Calculate the percentage of victim gold to be transferred
			const uint32 VictimLoot = static_cast<uint32>(VictimGold * PVPCorpseLootPercent);
			//only notify if a value is being transferred
			if (VictimLoot > 0){
				// Rifle the victim's corpse for loot
				killer->ModifyMoney(VictimLoot);
				victim->ModifyMoney(-VictimLoot);
				// Inform the player of the corpse loot
				Notify(killer, victim, nullptr, KILLTYPE_LOOT, VictimLoot);
			}
			// If enabled...
			if (PVPMultiplier > 0)
			{
				// No reward for killing yourself
				if (killer->GetGUID() == victim->GetGUID())
				{
					Notify(killer, victim, nullptr, KILLTYPE_SUICIDE, 0);
					return;
				}
				//Added in the KillerLevelDiff so bounties are not awarded for farming low level players.
				const int BountyAmount = ((VictimLevel * PVPMultiplier * KillerLevelDiff) / 3);
				// Pay the player the additional PVP bounty
				killer->ModifyMoney(BountyAmount);
				// Inform the player of the bounty amount
				Notify(killer, victim, nullptr, KILLTYPE_PVP, BountyAmount);
			}

	}}

	// Creature Kill Reward
	void OnPlayerCreatureKill(Player* player, Creature* killed)
	{
		// If enabled...
		if (sConfigMgr->GetOption<bool>(MFKEnable, true))
		{
			// Get the creature level
			const uint32 CreatureLevel = killed->GetLevel();
			uint32 BossMultiplier = 0;
			uint32 KillMultiplier = 0;
			KillType CreatureType;

			if (killed->IsDungeonBoss()) {
				BossMultiplier = sConfigMgr->GetOption<uint32>(MFKBountyKillDBMult, 0);
				CreatureType = KILLTYPE_DUNGEONBOSS;
			}
			else if (killed->isWorldBoss()) {
				BossMultiplier = sConfigMgr->GetOption<uint32>(MFKBountyKillWBMult, 0);
				CreatureType = KILLTYPE_WORLDBOSS;
			}
			else
			{
				KillMultiplier = sConfigMgr->GetOption<uint32>(MFKBountyKillMult, 0);
				CreatureType = KILLTYPE_MOB;
			}

			if (BossMultiplier > 0)
			{
				// Reward based on creature level
				const int BountyAmount = ((CreatureLevel * BossMultiplier) * 100);

				if (CreatureType == KILLTYPE_WORLDBOSS)
				{
					Notify(player, nullptr, killed, KILLTYPE_WORLDBOSS, BountyAmount);
					CreatureBounty(player, killed, KILLTYPE_MOB, BountyAmount);
				}
				else
				{
					CreatureBounty(player, killed, CreatureType, BountyAmount);
				}

			}

			if (KillMultiplier > 0)
			{
				// Reward based on creature level
				const int BountyAmount = ((CreatureLevel * KillMultiplier) / 3);

				// Pay the bounty amount
				CreatureBounty(player, killed, CreatureType, BountyAmount);
			}
		}
	}

	// Pay Creature Bounty
	void CreatureBounty(Player* player, Creature* killed, KillType kType, int bounty)
	{
		Group* group = player->GetGroup();
		const uint32 KillingBlowOnly = sConfigMgr->GetOption<uint32>(MFKBountyKillingBlow, 0);

		// Determine who receives the bounty
		if (!group || KillingBlowOnly == 1)
		{
			// Pay a specific player bounty amount
			player->ModifyMoney(bounty);

			// Inform the player of the bounty amount
			Notify(player, nullptr, killed, kType, bounty);
		}
		else
		{
			const uint32 MoneyForNothing = sConfigMgr->GetOption<uint32>(MFKBountyMoneyForNothing, 0);
			Group::MemberSlotList const& members = group->GetMemberSlots();

			// Pay the group (OnCreatureKill only rewards the player that got the killing blow)
			for (auto itr = members.begin(); itr != members.end(); ++itr)
			{
				Player* playerInGroup = ObjectAccessor::FindPlayer((*itr).guid);

				// Pay each player in the group
				if (playerInGroup && playerInGroup->GetSession())
				{
					// Money for nothing and the kills for free..
					if (MoneyForNothing == 1)
					{
						// Pay the bounty
						playerInGroup->ModifyMoney(bounty);

						// Inform the player of the bounty amount
						Notify(playerInGroup, nullptr, killed, kType, bounty);
					}
					else
					{
						// Only pay players that are in reward distance
						if (playerInGroup->IsAtGroupRewardDistance(killed))
						{
							// Pay the bounty
							playerInGroup->ModifyMoney(bounty);

							// Inform the player of the bounty amount
							Notify(playerInGroup, nullptr, killed, kType, bounty);
						}
					}
				}
			}
		}
	}

	void Notify(Player * killer, Player * victim, Creature * killed, KillType kType, int reward)
	{
		int rewardBreakdown[3];
		rewardBreakdown[0] = reward / 10000;
		reward = reward - rewardBreakdown[0] * 10000;
		rewardBreakdown[1] = reward / 100;
		rewardBreakdown[2] = reward - (rewardBreakdown[1] * 100);

		std::string rewardMsg = "";
		std::string victimMsg = "";
		std::string rewardVal = BuildRewardString(&rewardBreakdown[0]);
		switch (kType)
		{
		case KILLTYPE_LOOT:
			rewardMsg.append("You loot").append(rewardVal).append(" from the corpse.");
			victimMsg.append(killer->GetName()).append(" rifles through your corpse and takes").append(rewardVal).append(".");
			ChatHandler(victim->GetSession()).SendSysMessage(victimMsg.c_str());
			ChatHandler(killer->GetSession()).SendSysMessage(rewardMsg.c_str());
			break;
		case KILLTYPE_PVP:
			if (sConfigMgr->GetOption<bool>(MFKAnnouncePvP, true))
			{
				rewardMsg.append("|cff676767[ |cffFFFF00World |cff676767]|r:|cff4CFF00 ").append(killer->GetName()).append(" |cffFF0000has slain ");
				rewardMsg.append(victim->GetName()).append(" earning a bounty of").append(rewardVal).append(".");
				sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, rewardMsg.c_str());
			}
			break;
		case KILLTYPE_DUNGEONBOSS:
			if (sConfigMgr->GetOption<bool>(MFKAnnounceDungeonBoss, true))
			{
				rewardMsg.append("|cffFF8000Your group has defeated |cffFF0000").append(killed->GetName()).append("|cffFF8000.");
				ChatHandler(killer->GetSession()).SendSysMessage(rewardMsg.c_str());
				rewardMsg.clear();
			}
			break;
		case KILLTYPE_WORLDBOSS:
			if (sConfigMgr->GetOption<bool>(MFKAnnounceWorldBoss, true))
			{
				rewardMsg.append("|cffFF0000[ |cffFFFF00World |cffFF0000]|r:|cff4CFF00 ").append(killer->GetName());
				rewardMsg.append("'s|r group triumphed victoriously over |CFF18BE00[").append(killed->GetName()).append("]|r !");
				sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, rewardMsg.c_str());
				rewardMsg.clear();
			}
			break;
		case KILLTYPE_MOB:
			break;
		case KILLTYPE_SUICIDE:
			std::string message = "|cff4CFF00 ";
			message.append(killer->GetName());
			message.append(" met an untimely demise!");

			if (sConfigMgr->GetOption<bool>(MFKAnnounceWorldSuicide, true))
				sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message.c_str());

			if (sConfigMgr->GetOption<bool>(MFKAnnounceGuildSuicide, false))
			{
				Guild* guild = killer->GetGuild();
				if (guild)
					guild->BroadcastToGuild(killer->GetSession(), false, message, LANG_UNIVERSAL);
			}


			if (sConfigMgr->GetOption<bool>(MFKAnnounceGroupSuicide, false))
			{
				Group* group = killer->GetGroup();
				if (group)
				{
					Group::MemberSlotList const& members = group->GetMemberSlots();

					for (auto itr = members.begin(); itr != members.end(); ++itr)
					{
						Player* playerInGroup = ObjectAccessor::FindPlayer((*itr).guid);
						if (playerInGroup)
							ChatHandler(playerInGroup->GetSession()).SendSysMessage(message.c_str());
					}
				}
			}

		}

		if (kType != KILLTYPE_LOOT && kType != KILLTYPE_WORLDBOSS && kType != KILLTYPE_SUICIDE)
		{
			rewardMsg.clear();
			rewardMsg.append("You receive a bounty of");
			rewardMsg.append(rewardVal);
			rewardMsg.append(" for the kill.");
			ChatHandler(killer->GetSession()).SendSysMessage(rewardMsg.c_str());
		}
	}

	std::string BuildRewardString(int * reward)
	{
		std::string currSymbol[3] = { " gold", " silver", " copper" };
		std::string rewardMsg = "";
		for (int i = 0; i < 3; i++)
		{
			if (reward[i] > 0)
			{
				rewardMsg.append(" ").append(std::to_string(reward[i]));
				rewardMsg.append(currSymbol[i]);
			}
		}

		return rewardMsg;
	}
};

void AddMoneyForKillsScripts()
{
	new MoneyForKills();
}
