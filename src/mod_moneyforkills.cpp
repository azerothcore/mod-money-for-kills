/*

# Money For Kills #

#### A module for AzerothCore by [StygianTheBest](https://github.com/StygianTheBest/AzerothCore-Content/tree/master/Modules)
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


### License ###
------------------------------------------------------------------------------------------------------------------
- This code and content is released under the [GNU AGPL v3](https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3).

*/

#include "Config.h"
#include "Group.h"

enum KillType
{
    KILLTYPE_LOOT,
    KILLTYPE_PVP,
    KILLTYPE_DUNGEONBOSS,
    KILLTYPE_WORLDBOSS,
    KILLTYPE_MOB
};

class MoneyForKills : public PlayerScript
{
public:
    MoneyForKills() : PlayerScript("MoneyForKills") { }

    // Announce Module
    void OnLogin(Player *player) {
        if (sConfigMgr->GetBoolDefault("MFK.Enable", true))
        {
            if (sConfigMgr->GetBoolDefault("MFK.Announce", true))
            {
                ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00MoneyForKills |rmodule.");
            }
        }
    }

    // Player Kill Reward
    void OnPVPKill(Player* killer, Player* victim)
    {
        // If enabled...
        if (sConfigMgr->GetBoolDefault("MFK.Enable", true))
        {
            const uint32 PVPMultiplier = sConfigMgr->GetIntDefault("MFK.PVP.Multiplier", 0);

            // If enabled...
            if (PVPMultiplier > 0)
            {
                // No reward for killing yourself
                if (killer->GetGUID() == victim->GetGUID())
                {
                    // Inform the world
                    std::string message = "|cff676767[ |cffFFFF00World |cff676767]|r:|cff4CFF00 ";
                    message.append(killer->GetName());
                    message.append(" met an untimely demise!");

                    sWorld->SendServerMessage(SERVER_MSG_STRING, message.c_str());
                    return;
                }

                // Calculate the amount of gold to give to the victor
                const uint32 PVPCorpseLootPercent = sConfigMgr->GetIntDefault("MFK.PVP.CorpseLootPercent", 5);
                const uint32 VictimLevel = victim->getLevel();
                const int VictimLoot = (victim->GetMoney() * PVPCorpseLootPercent) / 100;
                const int BountyAmount = ((VictimLevel * PVPMultiplier) / 3);

                // Pay the player the additional PVP bounty
                killer->ModifyMoney(BountyAmount);

                // Rifle the victim's corpse for loot
                if (victim->GetMoney() >= 10000)
                {
                    // Player loots a percentage of the victim's gold
                    killer->ModifyMoney(VictimLoot);
                    victim->ModifyMoney(-VictimLoot);

                    // Inform the player of the corpse loot
                    Notify(killer, victim, nullptr, KILLTYPE_LOOT, VictimLoot);
                }

                // Inform the player of the bounty amount
                Notify(killer, victim, nullptr, KILLTYPE_PVP, BountyAmount);

                return;
            }
        }
    }

    // Creature Kill Reward
    void OnCreatureKill(Player* player, Creature* killed)
    {
        // If enabled...
        if (sConfigMgr->GetBoolDefault("MFK.Enable", true))
        {
            // Get the creature level
            const uint32 CreatureLevel = killed->getLevel();

            // What did the player kill?
            if (killed->IsDungeonBoss() || killed->isWorldBoss())
            {
                uint32 BossMultiplier;

                // Dungeon Boss or World Boss multiplier?
                if (killed->IsDungeonBoss())
                {
                    BossMultiplier = sConfigMgr->GetIntDefault("MFK.DungeonBoss.Multiplier", 0);
                }
                else
                {
                    BossMultiplier = sConfigMgr->GetIntDefault("MFK.WorldBoss.Multiplier", 0);
                }

                // If enabled...
                if (BossMultiplier > 0)
                {
                    // Reward based on creature level
                    const int BountyAmount = ((CreatureLevel * BossMultiplier) * 100);

                    if (killed->IsDungeonBoss())
                    {
                        // Pay the bounty amount
                        CreatureBounty(player, killed, KILLTYPE_DUNGEONBOSS, BountyAmount);
                    }
                    else
                    {
                        // Pay the bounty amount
                        CreatureBounty(player, killed, KILLTYPE_WORLDBOSS, BountyAmount);
                    }
                }
            }
            else
            {
                const uint32 KillMultiplier = sConfigMgr->GetIntDefault("MFK.Kill.Multiplier", 0);

                // If enabled...
                if (KillMultiplier > 0)
                {
                    // Reward based on creature level
                    const int BountyAmount = ((CreatureLevel * KillMultiplier) / 3);

                    // Pay the bounty amount
                    CreatureBounty(player, killed, KILLTYPE_MOB, BountyAmount);
                }
            }
        }
    }

    // Pay Creature Bounty
    void CreatureBounty(Player* player, Creature* killed, KillType kType, int bounty)
    {
        Group* group = player->GetGroup();
        Group::MemberSlotList const &members = group->GetMemberSlots();
        const uint32 KillingBlowOnly = sConfigMgr->GetIntDefault("MFK.KillingBlowOnly", 0);
        const uint32 MoneyForNothing = sConfigMgr->GetIntDefault("MFK.MoneyForNothing", 0);

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
            // Pay the group (OnCreatureKill only rewards the player that got the killing blow)
            for (Group::MemberSlotList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
            {
                Group::MemberSlot const &slot = *itr;
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
                victimMsg.append(killer->GetName()).append(" rifles through your corpse and takes").append(rewardVal);
                victimMsg.append(".");
                ChatHandler(victim->GetSession()).SendSysMessage(victimMsg.c_str());
                break;     
            case KILLTYPE_PVP:
                rewardMsg.append("|cff676767[ |cffFFFF00World |cff676767]|r:|cff4CFF00 ").append(killer->GetName()).append(" |cffFF0000has slain ");
                rewardMsg.append(victim->GetName()).append(" earning a bounty of").append(rewardVal).append(".");
                sWorld->SendServerMessage(SERVER_MSG_STRING, rewardMsg.c_str());
                break;
            case KILLTYPE_DUNGEONBOSS:
                rewardMsg.append("|cffFF8000Your group has defeated |cffFF0000").append(killed->GetName()).append("|cffFF8000.");
                ChatHandler(killer->GetSession()).SendSysMessage(rewardMsg.c_str());
                rewardMsg.clear();
                break;
            case KILLTYPE_WORLDBOSS:
                rewardMsg.append("|cffFF0000[ |cffFFFF00World |cffFF0000]|r:|cff4CFF00 ").append(killer->GetName());
                rewardMsg.append("'s|r group triumphed victoriously over |CFF18BE00[").append(killed->GetName()).append("]|r !");
                sWorld->SendServerMessage(SERVER_MSG_STRING, rewardMsg.c_str());
                rewardMsg.clear();
                break;
            case KILLTYPE_MOB:
                break;
        }

        if (kType != KILLTYPE_PVP)
        {
            
            rewardMsg.append(" You receive a bounty of");
            rewardMsg.append(rewardVal);
            ChatHandler(killer->GetSession()).SendSysMessage(rewardMsg.c_str());
            rewardMsg.append(" for the kill.");
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

class MoneyForKillsWorld : public WorldScript
{
public:
    MoneyForKillsWorld() : WorldScript("MoneyForKillsWorld") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (!reload) {
            std::string conf_path = _CONF_DIR;
            std::string cfg_file = conf_path + "/mod_moneyforkills.conf";
            
			#ifdef WIN32
			cfg_file = "mod_moneyforkills.conf";
			#endif
			
			std::string cfg_def_file = cfg_file + ".dist";
            sConfigMgr->LoadMore(cfg_def_file.c_str());
            sConfigMgr->LoadMore(cfg_file.c_str());
        }
    }
};

void AddMoneyForKillsScripts()
{
    new MoneyForKills();
	new MoneyForKillsWorld();
}
