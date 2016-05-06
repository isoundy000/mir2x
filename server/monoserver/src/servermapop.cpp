/*
 * =====================================================================================
 *
 *       Filename: servermapop.cpp
 *        Created: 05/03/2016 20:21:32
 *  Last Modified: 05/03/2016 22:25:39
 *
 *    Description: 
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#include "monster.hpp"
#include "actorpod.hpp"
#include "servermap.hpp"
#include "monoserver.hpp"

void ServerMap::On_MPK_HI(const MessagePack &, const Theron::Address &)
{
    m_Metronome = new Metronome(100);
    // m_ActorPod->Forward(MPK_HI, rstFromAddr);
    m_Metronome->Activate(m_ActorPod->GetAddress());
}

void ServerMap::On_MPK_METRONOME(const MessagePack &, const Theron::Address &)
{
    for(auto &rstRecordV: m_RegionMonitorRecordV2D){
        for(auto &rstRecord: rstRecordV){
            if(rstRecord.Valid()){
                m_ActorPod->Forward(MPK_METRONOME, rstRecord.PodAddress);
            }
        }
    }
}

void ServerMap::On_MPK_REGIONMONITORREADY(const MessagePack &rstMPK, const Theron::Address &)
{
    AMRegionMonitorReady stAMMR;
    std::memcpy(&stAMMR, rstMPK.Data(), sizeof(stAMMR));
    m_RegionMonitorRecordV2D[stAMMR.LocY][stAMMR.LocX].CanRun = true;
    CheckRegionMonitorReady();

    // if(RegionMonitorReady()){
    //     m_ActorPod->Forward(MPK_READY, m_CoreAddress);
    // }
}

void ServerMap::On_MPK_ADDMONSTER(const MessagePack &rstMPK, const Theron::Address &rstFromAddr)
{
    AMAddMonster stAMAM;
    std::memcpy(&stAMAM, rstMPK.Data(), sizeof(stAMAM));

    if(!ValidP(stAMAM.X, stAMAM.Y) && stAMAM.Strict){
        extern MonoServer *g_MonoServer;
        g_MonoServer->AddLog(LOGTYPE_WARNING, "invalid monster adding request");
    }else{
        Monster *pNewMonster = 
            new Monster(stAMAM.MonsterIndex, stAMAM.UID, stAMAM.AddTime);
        AMNewMonster stAMNM;
        stAMNM.Data = pNewMonster;

        if(!RegionMonitorReady()){
            CreateRegionMonterV2D();

            if(!RegionMonitorReady()){
                extern MonoServer *g_MonoServer;
                g_MonoServer->AddLog(LOGTYPE_WARNING,
                        "create region monitors for server map failed");
                g_MonoServer->Restart();
            }
        }

        auto stAddr = RegionMonitorAddressP(stAMAM.X, stAMAM.Y);
        if(stAddr == Theron::Address::Null()){
            extern MonoServer *g_MonoServer;
            g_MonoServer->AddLog(LOGTYPE_WARNING, "invalid location for new monstor");
            m_ActorPod->Forward(MPK_ERROR, rstFromAddr,rstMPK.ID());
        }else{
            auto fnROP = [this, rstFromAddr, nID = rstMPK.ID()](
                    const MessagePack &rstRMPK, const Theron::Address &){
                switch(rstRMPK.Type()){
                    case MPK_OK:
                        {
                            m_ActorPod->Forward(MPK_OK, rstFromAddr, nID);
                            break;
                        }
                    default:
                        {
                            m_ActorPod->Forward(MPK_ERROR, rstFromAddr, nID);
                            break;
                        }
                }
            };
            m_ActorPod->Forward({MPK_NEWMONSTER, stAMNM}, stAddr, fnROP);
        }
    }
}

void ServerMap::On_MPK_NEWMONSTER(const MessagePack &rstMPK, const Theron::Address &)
{
    AMNewMonster stAMNM;
    std::memcpy(&stAMNM, rstMPK.Data(), sizeof(stAMNM)); 
    // 1. create the monstor
    auto pNewMonster = new Monster(stAMNM.GUID, stAMNM.UID, stAMNM.AddTime);
    uint64_t nKey = ((uint64_t)stAMNM.UID << 32) + stAMNM.AddTime;

    // 2. put it in the pool
    m_CharObjectM[nKey] = pNewMonster;

    // 3. add the pointer inside and forward this message to the monitor
    stAMNM.Data = (void *)pNewMonster;
    auto stAddr = RegionMonitorAddressP(stAMNM.X, stAMNM.Y);
    m_ActorPod->Forward({MPK_NEWMONSTER, stAMNM}, stAddr);
}