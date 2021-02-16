/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 Stanford University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Serhat Arslan <sarslan@stanford.edu>
 */
#include <unordered_map>
#include <tuple>
#include <list>
#include <numeric>
#include <functional>
#include <algorithm>

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "node.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-header.h"
#include "nanopu-archt.h"
#include "ns3/nanopu-app-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NanoPuArcht");

NS_OBJECT_ENSURE_REGISTERED (NanoPuArcht);
    
/*
 * \brief Find the first set bit in the provided bitmap
 * 
 * \returns Index of first 1 from right to left, in binary representation of a bitmap
 */
uint16_t getFirstSetBitPos(bitmap_t n) { 
//   return ((n!=0) ? log2(n & -n) : BITMAP_SIZE); 
  return n._Find_first();
};
    
bitmap_t setBitMapUntil(uint16_t n) {
  NS_ASSERT_MSG(n <= BITMAP_SIZE, 
                "setBitMapUntil is called for n=" << n);
  bitmap_t bm;
  return ~bm >> (BITMAP_SIZE-n);
};
    
/******************************************************************************/
    
TypeId NanoPuArchtEgressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtEgressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtEgressPipe::NanoPuArchtEgressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}

NanoPuArchtEgressPipe::~NanoPuArchtEgressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void NanoPuArchtEgressPipe::EgressPipe (Ptr<const Packet> p, egressMeta_t meta)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
  NS_LOG_ERROR (Simulator::Now ().GetNanoSeconds () <<
                "ERROR: NanoPU wants to send a packet of size " << 
                p->GetSize () << ", but the egress pipe can not be found!");
    
  return;
}
    
/******************************************************************************/
    
TypeId NanoPuArchtArbiter::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtArbiter")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtArbiter::NanoPuArchtArbiter (Ptr<NanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
  
  m_pqInsertionOrder = 0;
}

NanoPuArchtArbiter::~NanoPuArchtArbiter ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void NanoPuArchtArbiter::SetEgressPipe (Ptr<NanoPuArchtEgressPipe> egressPipe)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_egressPipe = egressPipe;
}
    
void NanoPuArchtArbiter::Receive(Ptr<Packet> p, egressMeta_t meta)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  if (m_nanoPuArcht->ArbiterQueueEnabled())
  {
    arbiterMeta_t arbiterMeta = {};
    arbiterMeta.p = p;
    arbiterMeta.egressMeta = meta;
    arbiterMeta.insertionOrder = m_pqInsertionOrder;
      
    m_pq.push(arbiterMeta);
    m_pqInsertionOrder++;
      
    m_nanoPuArcht->SetNArbiterPackets (m_nanoPuArcht->GetNArbiterPackets ()+1);
    m_nanoPuArcht->SetNArbiterBytes (m_nanoPuArcht->GetNArbiterBytes () + p->GetSize());
      
    if (m_nextTxEvent.IsExpired())
      this->TxPkt ();
  }
  else
    m_egressPipe->EgressPipe(p, meta);
}
    
void NanoPuArchtArbiter::EmitAfterPktOfSize (uint32_t size)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << size);
    
  if (!m_nanoPuArcht->ArbiterQueueEnabled())
    return;
   
  if (m_nextTxEvent.IsExpired())
  {
    Time delay = m_nanoPuArcht->GetNicRate ().CalculateBytesTxTime (size);
//     delay -= NanoSeconds(1); // Accounts for rounding errors
      
    m_nextTxEvent = Simulator::Schedule (delay, &NanoPuArchtArbiter::TxPkt, this);
     
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () <<
                 " Arbiter delay = " << delay.GetNanoSeconds() <<
                 " PacketSize = " << size <<
                 " NicRate = " << m_nanoPuArcht->GetNicRate ());
  }
}
    
void NanoPuArchtArbiter::TxPkt()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  
  if (m_pq.empty())
  {
    m_pqInsertionOrder = 0;
    return;
  }
    
  arbiterMeta_t arbiterMeta = m_pq.top();
  m_pq.pop();
    
  m_nanoPuArcht->SetNArbiterPackets (m_nanoPuArcht->GetNArbiterPackets ()-1);
  m_nanoPuArcht->SetNArbiterBytes (m_nanoPuArcht->GetNArbiterBytes () 
                                   - arbiterMeta.p->GetSize());
    
  m_egressPipe->EgressPipe(arbiterMeta.p, arbiterMeta.egressMeta);
}
    
/******************************************************************************/
    
TypeId NanoPuArchtPacketize::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtPacketize")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtPacketize::NanoPuArchtPacketize (Ptr<NanoPuArcht> nanoPuArcht,
                                            Ptr<NanoPuArchtArbiter> arbiter)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
  m_arbiter = arbiter;
    
  m_txMsgIdFreeList.resize(m_nanoPuArcht->GetMaxNMessages ());
  std::iota(m_txMsgIdFreeList.begin(), m_txMsgIdFreeList.end(), 0);
}

NanoPuArchtPacketize::~NanoPuArchtPacketize ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void NanoPuArchtPacketize::SetTimerModule (Ptr<NanoPuArchtEgressTimer> timer)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_timer = timer;
}
    
void NanoPuArchtPacketize::DeliveredEvent (uint16_t txMsgId, uint16_t msgLen,
                                           bitmap_t ackPkts)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
               " NanoPU DeliveredEvent for msg " << txMsgId);
  NS_LOG_LOGIC("\tDelivered packets (bitmap): " << 
               std::bitset<BITMAP_SIZE>(ackPkts) );
    
  if (m_deliveredBitmap.find(txMsgId) != m_deliveredBitmap.end() \
      && std::find(m_txMsgIdFreeList.begin(),m_txMsgIdFreeList.end(),txMsgId) == m_txMsgIdFreeList.end())
  {
    m_deliveredBitmap[txMsgId] |= ackPkts;
    m_ranks[txMsgId] = msgLen -  m_deliveredBitmap[txMsgId].count();
      
//     if (m_deliveredBitmap[txMsgId] == (((bitmap_t)1)<<msgLen)-1)
//     if (m_ranks[txMsgId] == 0) // More efficient, but bug prone
    if (m_deliveredBitmap[txMsgId] == setBitMapUntil(msgLen))
    {
      NS_LOG_INFO("The whole message is delivered.");
        
      this->ClearStateForMsg (txMsgId);
    }
  }
  else
  {
    NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                  " ERROR: DeliveredEvent was triggered for unknown tx_msg_id: "
                  << txMsgId);
  }
}
    
void NanoPuArchtPacketize::CreditToBtxEvent (uint16_t txMsgId, int rtxPkt, 
                                             int newCredit, int compVal, 
                                             CreditEventOpCode_t opCode, 
                                             std::function<bool(int,int)> relOp)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << rtxPkt << newCredit << compVal);
    
  NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () << 
               " NanoPU CreditToBtxEvent for msg " << txMsgId );
    
  if (std::find(m_txMsgIdFreeList.begin(),m_txMsgIdFreeList.end(),txMsgId) == m_txMsgIdFreeList.end())
  {
    if (rtxPkt != -1 && m_toBeTxBitmap.find(txMsgId) != m_toBeTxBitmap.end())
    {
      NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () <<
                   " Marking msg " << txMsgId << ", pkt " << rtxPkt <<
                   " for retransmission.");
      m_toBeTxBitmap[txMsgId].set(rtxPkt);
    }
      
    if (newCredit != -1 && m_credits.find(txMsgId) != m_credits.end())
    {
      uint16_t curCredit = m_credits[txMsgId];
      if (relOp(compVal,curCredit))
      {
        if (opCode == CreditEventOpCode_t::WRITE)
        {
          m_credits[txMsgId] = newCredit;
        }
        else if (opCode == CreditEventOpCode_t::ADD)
        {
          m_credits[txMsgId] += newCredit;
        }
        else if (opCode == CreditEventOpCode_t::SHIFT_RIGHT)
        {
          m_credits[txMsgId] >>= newCredit;
        }
          
        m_credits[txMsgId] = std::min((int)m_credits[txMsgId], (int)BITMAP_SIZE);
        NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () <<
                    " Changed credit for msg " << txMsgId <<
                    " from " << curCredit << " to " << m_credits[txMsgId]);
      }
    }
      
    if (m_toBeTxBitmap.find(txMsgId) != m_toBeTxBitmap.end())
    {
//       bitmap_t txPkts = m_toBeTxBitmap[txMsgId] & ((((bitmap_t)1)<<m_credits[txMsgId])-1);
      bitmap_t txPkts = m_toBeTxBitmap[txMsgId] & setBitMapUntil(m_credits[txMsgId]);
    
      if (txPkts.any()) 
      {
        bool isRtx = false;
        bool isNewMsg = false;
        Dequeue (txMsgId, txPkts, isRtx, isNewMsg);
        m_toBeTxBitmap[txMsgId] &= ~txPkts;
      }
    }
  }
  else
  {
    NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                  " ERROR: CreditToBtxEvent was triggered for unknown tx_msg_id: "
                  << txMsgId);
  }
}
    
void NanoPuArchtPacketize::TimeoutEvent (uint16_t txMsgId, uint16_t rtxOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () << 
               " NanoPU Timeout for msg " << txMsgId << 
               " in Packetization Buffer");
    
  if (std::find(m_txMsgIdFreeList.begin(),m_txMsgIdFreeList.end(),txMsgId) == m_txMsgIdFreeList.end())
  {
    if (m_timeoutCnt[txMsgId] >= m_nanoPuArcht->GetMaxTimeoutCnt ())
    {
      NS_LOG_WARN(Simulator::Now ().GetNanoSeconds () <<
                  " Outbound Msg " << txMsgId << " expired. "
                  "(MsgSize = " << m_appHeaders[txMsgId].GetMsgLen () << ")");
        
      this->ClearStateForMsg (txMsgId);
    }
    else
    {
      if (m_maxTxPktOffset[txMsgId] > rtxOffset)
        m_timeoutCnt[txMsgId] = 0;
      else
        m_timeoutCnt[txMsgId] ++;
        
//       bitmap_t rtxPkts = (~m_deliveredBitmap[txMsgId]) & ((((bitmap_t)1)<<(rtxOffset+1))-1);
      bitmap_t rtxPkts = (~m_deliveredBitmap[txMsgId]) & setBitMapUntil(rtxOffset+1);
      
      if (rtxPkts.any()) 
      { 
        NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () <<
                   " NanoPU will retransmit " << std::bitset<BITMAP_SIZE>(rtxPkts) );
        
        bool isRtx = true;
        bool isNewMsg = false;
        Dequeue (txMsgId, rtxPkts, isRtx, isNewMsg);
        m_toBeTxBitmap[txMsgId] &= ~rtxPkts;
      }
        
      m_timer->RescheduleTimerEvent (txMsgId, m_maxTxPktOffset[txMsgId]);
    }
  }
  else
  {
    NS_LOG_WARN(Simulator::Now ().GetNanoSeconds () <<
                " TimeoutEvent was triggered for unknown tx_msg_id: "
                << txMsgId);
  }
}
    
int NanoPuArchtPacketize::ProcessNewMessage (Ptr<Packet> msg)
{
  Ptr<Packet> cmsg = msg->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cmsg);
    
  NanoPuAppHeader apphdr;
  cmsg->RemoveHeader (apphdr);
    
  NS_ASSERT_MSG (apphdr.GetHeaderType() == NANOPU_APP_HEADER_TYPE, 
                 "NanoPU expects packets to have NanoPU App Header!");
    
  uint16_t msgSize = apphdr.GetPayloadSize();
  uint16_t payloadSize = m_nanoPuArcht->GetPayloadSize ();
  NS_ASSERT_MSG (msgSize / payloadSize + (msgSize % payloadSize != 0) < (uint16_t)BITMAP_SIZE,
                 "NanoPU can not handle messages larger than "
                 << BITMAP_SIZE << " packets!");
  
  uint16_t txMsgId;
  if (m_txMsgIdFreeList.size() > 0)
  {
    txMsgId = m_txMsgIdFreeList.front ();
    NS_LOG_INFO("NanoPU Packetization Buffer allocating txMsgId: " << txMsgId);
    m_txMsgIdFreeList.pop_front ();
    
    uint32_t remainingBytes = cmsg->GetSize ();
    NS_ASSERT_MSG (msgSize == (uint16_t) remainingBytes,
                   "The payload size in the NanoPU App header doesn't match real payload size. " <<
                   msgSize << " vs " << (uint16_t) remainingBytes);
    m_appHeaders[txMsgId] = apphdr;
     
    std::map<uint16_t,Ptr<Packet>> buffer;
    uint16_t numPkts = 0;
    uint32_t nextPktSize = remainingBytes;
    while (remainingBytes > 0)
    {
      nextPktSize = std::min(remainingBytes, (uint32_t) payloadSize);
        
      if (!m_nanoPuArcht->MemIsOptimized ())
        buffer[numPkts] = cmsg->CreateFragment (cmsg->GetSize () - remainingBytes, 
                                                    nextPktSize);
      remainingBytes -= nextPktSize;
      numPkts ++;
    }
    if (m_nanoPuArcht->MemIsOptimized ())
      m_lastPktSize[txMsgId] = nextPktSize;  
    else
      m_buffers[txMsgId] = buffer;
      
    NS_ASSERT_MSG (apphdr.GetMsgLen() == numPkts,
                   "The message length in the NanoPU App header "
                   "doesn't match number of packets for this message.");
      
    uint16_t requestedCredit = apphdr.GetInitWinSize (); 
    uint16_t initialCredit = m_nanoPuArcht->GetInitialCredit ();
    if (requestedCredit != 0)
      m_credits[txMsgId] = (requestedCredit < initialCredit) ? requestedCredit : initialCredit;
    else
      m_credits[txMsgId] = initialCredit;
      
    m_deliveredBitmap[txMsgId] = 0;
      
    m_toBeTxBitmap[txMsgId] = setBitMapUntil(numPkts);
      
    m_maxTxPktOffset[txMsgId] = 0;
      
    m_timeoutCnt[txMsgId] = 0;
      
    m_ranks[txMsgId] = numPkts;
    
    m_timer->ScheduleTimerEvent (txMsgId, 0);
      
//     bitmap_t txPkts = m_toBeTxBitmap[txMsgId] & ((((bitmap_t)1)<<m_credits[txMsgId])-1);
    bitmap_t txPkts = m_toBeTxBitmap[txMsgId] & setBitMapUntil(m_credits[txMsgId]);
    bool isRtx = false;
    bool isNewMsg = true;
    Dequeue (txMsgId, txPkts, isRtx, isNewMsg);
      
    m_toBeTxBitmap[txMsgId] &= ~txPkts;
    
    return txMsgId;
  }
  else
  {
    NS_LOG_ERROR(Simulator::Now ().GetNanoSeconds () << 
                 " ERROR: NanoPU could not allocate a new"
                 " txMsgId for the new message. " << 
                 this << " " << msg);
    return -1;
  }
}
    
void NanoPuArchtPacketize::Dequeue (uint16_t txMsgId, bitmap_t txPkts, 
                                    bool isRtx, bool isNewMsg)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << txMsgId);
  
  NanoPuAppHeader apphdr = m_appHeaders[txMsgId];
  uint16_t msgLen = apphdr.GetMsgLen();
    
  egressMeta_t meta = {};
  meta.containsData = true;
  meta.txMsgId = txMsgId;
  meta.msgLen = msgLen;
  meta.remoteIp = apphdr.GetRemoteIp();
  meta.remotePort = apphdr.GetRemotePort();
  meta.localPort = apphdr.GetLocalPort();
  meta.rank = m_ranks[txMsgId];
    
  uint16_t pktOffset = getFirstSetBitPos(txPkts);
  while (pktOffset != BITMAP_SIZE)
  {
    NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () <<
                " NanoPU Packetization Buffer transmitting pkt " <<
                pktOffset << " from msg " << txMsgId);
    
    Ptr<Packet> p;
    if (m_nanoPuArcht->MemIsOptimized ())
    {
      if (pktOffset == msgLen-1)
        p = Create<Packet> (m_lastPktSize[txMsgId]); 
      else
        p = Create<Packet> (m_nanoPuArcht->GetPayloadSize ()); 
    }
    else
      p = m_buffers[txMsgId][pktOffset]->Copy ();
    
    meta.isNewMsg = isNewMsg;
    meta.isRtx = isRtx;
    meta.pktOffset = pktOffset;
      
    m_arbiter->Receive(p, meta);
      
    txPkts &= ~(((bitmap_t)1)<<pktOffset);
      
    if (pktOffset > m_maxTxPktOffset[txMsgId])
      m_maxTxPktOffset[txMsgId] = pktOffset;
    
    pktOffset = getFirstSetBitPos(txPkts);
  }
}
    
void NanoPuArchtPacketize::ClearStateForMsg (uint16_t txMsgId)
{
  NS_LOG_FUNCTION(Simulator::Now ().GetNanoSeconds () << this << txMsgId);
      
  m_timer->CancelTimerEvent (txMsgId);
        
  /* Clear the stored state for simulation performance */
  m_appHeaders.erase (txMsgId);
  m_deliveredBitmap.erase (txMsgId);
  m_credits.erase (txMsgId);
  m_toBeTxBitmap.erase (txMsgId);
  m_maxTxPktOffset.erase (txMsgId);
  m_timeoutCnt.erase (txMsgId);
  m_ranks.erase (txMsgId);
  
  if (m_nanoPuArcht->MemIsOptimized ())
  {
    m_lastPktSize.erase(txMsgId);
  }
  else
  {
//     for (auto it = m_buffers[txMsgId].begin(); it != m_buffers[txMsgId].end(); it++)
//     {
//       it->second->Unref();
//     }
    m_buffers[txMsgId].clear();
    m_buffers.erase(txMsgId);
  }
       
  /* Free the txMsgId*/
  m_txMsgIdFreeList.push_back (txMsgId);
}
    
/******************************************************************************/
    
TypeId NanoPuArchtEgressTimer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtEgressTimer")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtEgressTimer::NanoPuArchtEgressTimer (Ptr<NanoPuArcht> nanoPuArcht,
                                                Ptr<NanoPuArchtPacketize> packetize)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
  m_packetize = packetize;
}

NanoPuArchtEgressTimer::~NanoPuArchtEgressTimer ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void NanoPuArchtEgressTimer::ScheduleTimerEvent (uint16_t txMsgId, uint16_t rtxOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " NanoPU Egress ScheduleTimer Event for msg " << txMsgId <<
                " rtxOffset " << rtxOffset);
  
  m_timers[txMsgId] = Simulator::Schedule (m_nanoPuArcht->GetTimeoutInterval (),
                                           &NanoPuArchtEgressTimer::InvokeTimeoutEvent, 
                                           this, txMsgId, rtxOffset);
}
    
void NanoPuArchtEgressTimer::CancelTimerEvent (uint16_t txMsgId)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " NanoPU Egress CancelTimer Event for msg " << txMsgId);
  
  if (m_timers.find(txMsgId) != m_timers.end() )
  {
    Simulator::Cancel (m_timers[txMsgId]);
    m_timers.erase(txMsgId);
  }
}
    
void NanoPuArchtEgressTimer::RescheduleTimerEvent (uint16_t txMsgId, uint16_t rtxOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " NanoPU Egress RescheduleTimer Event for msg " << txMsgId <<
                " rtxOffset " << rtxOffset);
    
  m_timers[txMsgId] = Simulator::Schedule (m_nanoPuArcht->GetTimeoutInterval (),
                                           &NanoPuArchtEgressTimer::InvokeTimeoutEvent, 
                                           this, txMsgId, rtxOffset);
}
    
void NanoPuArchtEgressTimer::InvokeTimeoutEvent (uint16_t txMsgId, uint16_t rtxOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " NanoPU Egress InvokeTimeoutEvent Event for msg " << txMsgId <<
                " rtxOffset " << rtxOffset);
  
  m_timers.erase(txMsgId);
  m_packetize->TimeoutEvent (txMsgId, rtxOffset);
}
    
/******************************************************************************/

TypeId NanoPuArchtReassemble::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtReassemble")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtReassemble::NanoPuArchtReassemble (Ptr<NanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () 
                   << this << nanoPuArcht);
    
  m_nanoPuArcht = nanoPuArcht;
    
  m_rxMsgIdFreeList.resize(m_nanoPuArcht->GetMaxNMessages ());
  std::iota(m_rxMsgIdFreeList.begin(), m_rxMsgIdFreeList.end(), 0);
}

NanoPuArchtReassemble::~NanoPuArchtReassemble ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void NanoPuArchtReassemble::SetTimerModule (Ptr<NanoPuArchtIngressTimer> timer)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_timer = timer;
}

rxMsgInfoMeta_t 
NanoPuArchtReassemble::GetRxMsgInfo (Ipv4Address srcIp, uint16_t srcPort, 
                                     uint16_t txMsgId, uint16_t msgLen, 
                                     uint16_t pktOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << 
                   srcIp << srcPort << txMsgId << msgLen << pktOffset);
    
  rxMsgInfoMeta_t rxMsgInfo = {};
  rxMsgInfo.isNewMsg = false;
  rxMsgInfo.isNewPkt = false;
  rxMsgInfo.success = false;
    
  rxMsgIdTableKey_t key (srcIp.Get (), srcPort, txMsgId);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
                " NanoPU Reassembly Buffer processing GetRxMsgInfo"
                " extern call for: " << srcIp.Get () << "-" << 
                srcPort << "-" << txMsgId); 
    
  auto entry = m_rxMsgIdTable.find(key);
  if (entry != m_rxMsgIdTable.end())
  {
    rxMsgInfo.rxMsgId = entry->second;
      
    // compute the beginning of the inflight window
    rxMsgInfo.ackNo = getFirstSetBitPos(~(m_receivedBitmap[rxMsgInfo.rxMsgId]));
      
    rxMsgInfo.numPkts = m_receivedBitmap[rxMsgInfo.rxMsgId].count();
      
    rxMsgInfo.isNewPkt = !m_receivedBitmap[rxMsgInfo.rxMsgId].test(pktOffset);
      
    rxMsgInfo.success = true;
    NS_LOG_INFO("NanoPU Reassembly Buffer Found rxMsgId: " << entry->second <<
                " (ackNo: " << rxMsgInfo.ackNo << 
                " msgLen: " << msgLen << " numPkts: " << rxMsgInfo.numPkts <<
                " isNewPkt: " << rxMsgInfo.isNewPkt << ") ");
  }
  // try to allocate an rx_msg_id
  else if (m_rxMsgIdFreeList.size() > 0)
  {
    rxMsgInfo.rxMsgId = m_rxMsgIdFreeList.front ();
    NS_LOG_INFO("NanoPU Reassembly Buffer allocating rxMsgId: " 
                << rxMsgInfo.rxMsgId);
    m_rxMsgIdFreeList.pop_front ();
     
    m_rxMsgIdTable.insert({key,rxMsgInfo.rxMsgId});
    m_receivedBitmap.insert({rxMsgInfo.rxMsgId,0});
      
    if (m_nanoPuArcht->MemIsOptimized ())
    {
      m_lastPktSize.insert({rxMsgInfo.rxMsgId,0});
    }
    else
    {
      std::map<uint16_t,Ptr<Packet>> buffer;
      m_buffers.insert({rxMsgInfo.rxMsgId,buffer});
    }
    
    rxMsgInfo.ackNo = 0;
    rxMsgInfo.numPkts = 0;
    rxMsgInfo.isNewMsg = true;
    rxMsgInfo.isNewPkt = true;
    rxMsgInfo.success = true;
  }
    
  return rxMsgInfo;
}
    
void 
NanoPuArchtReassemble::ProcessNewPacket (Ptr<Packet> pkt, reassembleMeta_t meta)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << pkt);
    
  NS_ASSERT(meta.pktOffset <= meta.msgLen);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
                " NanoPU Reassembly Buffer processing pkt "<< meta.pktOffset 
                << " for msg " << meta.rxMsgId);
    
  if (m_receivedBitmap.find(meta.rxMsgId) != m_receivedBitmap.end ())
  {
    if (m_nanoPuArcht->MemIsOptimized ())
    {
      if (meta.pktOffset == meta.msgLen -1)
        m_lastPktSize [meta.rxMsgId] = pkt->GetSize();
    }
    else /* Record pkt in buffer*/
      m_buffers[meta.rxMsgId][meta.pktOffset] = pkt;
    
    /* Mark the packet as received*/
    // NOTE: received_bitmap must have 2 write ports: here and in getRxMsgInfo()
    m_receivedBitmap [meta.rxMsgId].set(meta.pktOffset);
    
    /* Check if all pkts have been received*/
    if (m_receivedBitmap [meta.rxMsgId] == setBitMapUntil(meta.msgLen))
    {
      NS_LOG_INFO ("All packets have been received for msg " << meta.rxMsgId);
      
      /* Push the reassembled msg to the applications*/
      Ptr<Packet> msg;
      if (m_nanoPuArcht->MemIsOptimized ())
      {
        uint32_t msgSize = (meta.msgLen-1) * m_nanoPuArcht->GetPayloadSize ();
        msgSize += m_lastPktSize [meta.rxMsgId];
        msg =  Create<Packet> (msgSize);
      }
      else
      {
        msg =  m_buffers[meta.rxMsgId][0];
        for (uint16_t i=1; i<meta.msgLen; i++)
        {
          msg->AddAtEnd (m_buffers[meta.rxMsgId][i]);
        }
      }
        
      NanoPuAppHeader apphdr;
      apphdr.SetHeaderType (NANOPU_APP_HEADER_TYPE);
      apphdr.SetRemoteIp (meta.srcIp);
      apphdr.SetRemotePort (meta.srcPort);
      apphdr.SetLocalPort (meta.dstPort);
      apphdr.SetMsgLen (meta.msgLen);
      apphdr.SetInitWinSize (m_nanoPuArcht->GetInitialCredit ());
      apphdr.SetPayloadSize (msg->GetSize ());
      msg->AddHeader (apphdr);
    
      m_nanoPuArcht->NotifyApplications (msg, (int)meta.txMsgId);
//       Simulator::Schedule (NanoSeconds(REASSEMBLE_DELAY), 
//                            &NanoPuArcht::NotifyApplications, 
//                            m_nanoPuArcht, msg, (int)meta.txMsgId);
      
//       /* Free the rxMsgId*/
//       rxMsgIdTableKey_t key (meta.srcIp.Get (), meta.srcPort, meta.txMsgId);
//       auto map = m_rxMsgIdTable.find (key);
//       NS_ASSERT (meta.rxMsgId == map->second);
//       m_rxMsgIdTable.erase (map);
        
      this->ClearStateForMsg(meta.rxMsgId);
    }
    else
    {
      m_timer->ScheduleTimerEvent (meta.rxMsgId);
    }
  }
  else
  {
    NS_LOG_WARN(Simulator::Now ().GetNanoSeconds () <<
                " NanoPuArchtReassemble (" << this <<
                ") can not find the rxMsgId (" << meta.rxMsgId <<
                ") any more!");
  }
}
    
void NanoPuArchtReassemble::TimeoutEvent (uint16_t rxMsgId)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this<< rxMsgId);
    
  NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () << 
               " NanoPU Timeout for msg " << rxMsgId << " in Reassembly Buffer");
    
  if (std::find(m_rxMsgIdFreeList.begin(),m_rxMsgIdFreeList.end(),rxMsgId) == m_rxMsgIdFreeList.end())
  {
    NS_LOG_WARN(Simulator::Now ().GetNanoSeconds () <<
                 " Inbound Msg " << rxMsgId << " expired.");
        
    this->ClearStateForMsg(rxMsgId);
  }
  else
  {
    NS_LOG_WARN(Simulator::Now ().GetNanoSeconds () <<
                 " TimeoutEvent was triggered for unknown rx_msg_id: "
                 << rxMsgId);
  }
}
    
void NanoPuArchtReassemble::ClearStateForMsg (uint16_t rxMsgId)
{
  NS_LOG_FUNCTION(Simulator::Now ().GetNanoSeconds () << this << rxMsgId);
      
  m_timer->CancelTimerEvent (rxMsgId);
    
  for (auto it = m_rxMsgIdTable.begin(); it != m_rxMsgIdTable.end(); ++it) 
  {
    if (it->second == rxMsgId)
    {
      m_rxMsgIdTable.erase (it);
      break;
    }
  }
      
  /* Clear the stored state for simulation performance */
  m_receivedBitmap.erase(rxMsgId);
  
  if (m_nanoPuArcht->MemIsOptimized ())
  {
    m_lastPktSize.erase(rxMsgId);
  }
  else
  {
//     for (auto it = m_buffers[rxMsgId].begin(); it != m_buffers[rxMsgId].end(); it++)
//     {
//       it->second->Unref();
//     }
    m_buffers[rxMsgId].clear();
    m_buffers.erase(rxMsgId);
  }
    
  m_rxMsgIdFreeList.push_back (rxMsgId);
}
 
/******************************************************************************/
    
TypeId NanoPuArchtIngressTimer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtIngressTimer")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtIngressTimer::NanoPuArchtIngressTimer (Ptr<NanoPuArcht> nanoPuArcht,
                                                  Ptr<NanoPuArchtReassemble> reassemble)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
  m_reassemble = reassemble;
}

NanoPuArchtIngressTimer::~NanoPuArchtIngressTimer ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void NanoPuArchtIngressTimer::ScheduleTimerEvent (uint16_t rxMsgId)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " NanoPU Ingress (Re)ScheduleTimer Event for msg " << rxMsgId);
    
  if (m_timers.find(rxMsgId) != m_timers.end() )
  {
    Simulator::Cancel (m_timers[rxMsgId]);
  }
  m_timers[rxMsgId] = Simulator::Schedule (m_nanoPuArcht->GetTimeoutInterval () *2,
                                           &NanoPuArchtIngressTimer::InvokeTimeoutEvent, 
                                           this, rxMsgId);
}
    
void NanoPuArchtIngressTimer::CancelTimerEvent (uint16_t rxMsgId)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " NanoPU Ingress CancelTimer Event for msg " << rxMsgId);
  
  if (m_timers.find(rxMsgId) != m_timers.end() )
  {
    Simulator::Cancel (m_timers[rxMsgId]);
    m_timers.erase(rxMsgId);
  }
}
       
void NanoPuArchtIngressTimer::InvokeTimeoutEvent (uint16_t rxMsgId)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " NanoPU Ingress InvokeTimeoutEvent Event for msg " << rxMsgId);
    
  m_reassemble->TimeoutEvent (rxMsgId);
  m_timers.erase(rxMsgId);
}
    
/******************************************************************************/
    
TypeId NanoPuArcht::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArcht")
    .SetParent<Object> ()
    .SetGroupName("Network")
    .AddConstructor<NanoPuArcht> ()
    .AddAttribute ("PayloadSize", 
                   "MTU for the network interface excluding the header sizes",
                   UintegerValue (1400),
                   MakeUintegerAccessor (&NanoPuArcht::m_payloadSize),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxNMessages", 
                   "Maximum number of messages NanoPU can handle at a time",
                   UintegerValue (100),
                   MakeUintegerAccessor (&NanoPuArcht::m_maxNMessages),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TimeoutInterval", "Time value to expire the timers",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&NanoPuArcht::m_timeoutInterval),
                   MakeTimeChecker (MicroSeconds (0)))
    .AddAttribute ("InitialCredit", "Initial window of packets to be sent",
                   UintegerValue (10),
                   MakeUintegerAccessor (&NanoPuArcht::m_initialCredit),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxNTimeouts", 
                   "Max allowed number of retransmissions before discarding a msg",
                   UintegerValue (5),
                   MakeUintegerAccessor (&NanoPuArcht::m_maxTimeoutCnt),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("OptimizeMemory", 
                   "High performant mode (only packet sizes are stored to save from memory).",
                   BooleanValue (true),
                   MakeBooleanAccessor (&NanoPuArcht::m_memIsOptimized),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableArbiterQueueing", 
                   "Enables priority queuing on Arbiter.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&NanoPuArcht::m_enableArbiterQueueing),
                   MakeBooleanChecker ())
    .AddTraceSource ("MsgBegin",
                     "Trace source indicating a message has been delivered to "
                     "the the NanoPuArcht by the sender application layer.",
                     MakeTraceSourceAccessor (&NanoPuArcht::m_msgBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MsgFinish",
                     "Trace source indicating a message has been delivered to "
                     "the receiver application by the NanoPuArcht layer.",
                     MakeTraceSourceAccessor (&NanoPuArcht::m_msgFinishTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PacketsInArbiterQueue",
                     "Number of packets currently stored in the arbiter queue",
                     MakeTraceSourceAccessor (&NanoPuArcht::m_nArbiterPackets),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("BytesInArbiterQueue",
                     "Number of bytes (without metadata) currently stored in the arbiter queue",
                     MakeTraceSourceAccessor (&NanoPuArcht::m_nArbiterBytes),
                     "ns3::TracedValueCallback::Uint32")
  ;
  return tid;
}

/*
 * NanoPu Architecture requires a transport module.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for constructor
 */
NanoPuArcht::NanoPuArcht ()
  : m_nArbiterPackets (0),
    m_nArbiterBytes (0)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}

/*
 * NanoPu Architecture requires a transport module.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for destructor
 */
NanoPuArcht::~NanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void NanoPuArcht::AggregateIntoDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << device);
    
  m_boundnetdevice = device;
  
  BindToNetDevice ();
  AggregateObject(m_boundnetdevice);
    
  m_arbiter = CreateObject<NanoPuArchtArbiter> (this);
    
  m_packetize = CreateObject<NanoPuArchtPacketize> (this, m_arbiter);
    
  m_egressTimer = CreateObject<NanoPuArchtEgressTimer> (this, m_packetize);
    
  m_packetize->SetTimerModule (m_egressTimer); 
    
  m_reassemble = CreateObject<NanoPuArchtReassemble> (this);
    
  m_ingressTimer = CreateObject<NanoPuArchtIngressTimer> (this, m_reassemble); 
    
  m_reassemble->SetTimerModule (m_ingressTimer);
}
    
/*
 * NanoPu Architecture requires a transport module. The function below
 * is a reference implementation for future transport modules.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for the real implementation.
 */
void NanoPuArcht::BindToNetDevice ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () 
                   << this << m_boundnetdevice);
    
  if (m_boundnetdevice != 0)
    {
      bool found = false;
      for (uint32_t i = 0; i < GetNode ()->GetNDevices (); i++)
        {
          if (GetNode ()->GetDevice (i) == m_boundnetdevice)
            {
              found = true;
              break;
            }
        }
      NS_ASSERT_MSG (found, "NanoPU cannot be bound to a"
                     " NetDevice not existing on the Node");
    }

  m_boundnetdevice->SetReceiveCallback (MakeCallback (&NanoPuArcht::EnterIngressPipe, this));
    
  Ptr<Ipv4> ipv4proto = m_boundnetdevice->GetNode ()->GetObject<Ipv4> ();
  int32_t ifIndex = ipv4proto->GetInterfaceForDevice (m_boundnetdevice);
  Ipv4Address dummyAddr ((uint32_t)0);
  m_localIp = ipv4proto->SourceAddressSelection (ifIndex, dummyAddr);
    
  PointToPointNetDevice* p2pNetDevice = dynamic_cast<PointToPointNetDevice*>(&(*(m_boundnetdevice))); 
  m_nicRate = p2pNetDevice->GetDataRate ();
}
    
Ptr<NetDevice>
NanoPuArcht::GetBoundNetDevice ()
{
  return m_boundnetdevice;
}
    
/* Returns associated node */
Ptr<Node>
NanoPuArcht::GetNode (void)
{
  return m_boundnetdevice->GetNode ();;
}
    
Ptr<NanoPuArchtReassemble> 
NanoPuArcht::GetReassemblyBuffer (void)
{
  return m_reassemble;
}
  
Ptr<NanoPuArchtPacketize> 
NanoPuArcht::GetPacketizationBuffer (void)
{
  return m_packetize;
}
    
Ptr<NanoPuArchtArbiter> 
NanoPuArcht::GetArbiter (void)
{
  return m_arbiter;
}
    
uint32_t
NanoPuArcht::GetNArbiterPackets (void) const
{
  return m_nArbiterPackets;
}
    
uint32_t
NanoPuArcht::GetNArbiterBytes (void) const
{
  return m_nArbiterBytes;
}
    
void
NanoPuArcht::SetNArbiterPackets (uint32_t nArbiterPackets)
{
  m_nArbiterPackets = nArbiterPackets;
}
    
void
NanoPuArcht::SetNArbiterBytes (uint32_t nArbiterBytes)
{
  m_nArbiterBytes = nArbiterBytes;
}
    
Ipv4Address
NanoPuArcht::GetLocalIp (void)
{
  return m_localIp;
}
    
DataRate
NanoPuArcht::GetNicRate (void)
{
  return m_nicRate;
}
    
uint16_t NanoPuArcht::GetPayloadSize (void)
{
  return m_payloadSize;
}
    
uint16_t NanoPuArcht::GetInitialCredit (void)
{
  return m_initialCredit;
}
    
uint16_t NanoPuArcht::GetMaxTimeoutCnt (void)
{
  return m_maxTimeoutCnt;
}
    
Time NanoPuArcht::GetTimeoutInterval (void)
{
  return m_timeoutInterval;
}
    
uint16_t NanoPuArcht::GetMaxNMessages (void)
{
  return m_maxNMessages;
}
    
bool NanoPuArcht::MemIsOptimized (void)
{
  return m_memIsOptimized;
}
    
bool NanoPuArcht::ArbiterQueueEnabled (void)
{
  return m_enableArbiterQueueing;
}
   
void NanoPuArcht::SetRecvCallback (Callback<void, Ptr<Packet> > reassembledMsgCb)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << 
                   this << &reassembledMsgCb);
  NS_ASSERT_MSG(m_reassembledMsgCb.IsNull (),
                "An application has already been installed on this NanoPU!");
  m_reassembledMsgCb = reassembledMsgCb;
}

/*
 * NanoPu Architecture requires a transport module. The function below
 * is a reference implementation for future transport modules.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for the real implementation.
 */
bool NanoPuArcht::EnterIngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                                    uint16_t protocol, const Address &from)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
                " NanoPU received a packet of size " << p->GetSize () <<
                ", but no transport protocol has been programmed.");
    
  return false;
}
    
bool NanoPuArcht::SendToNetwork (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
  NS_ASSERT_MSG (m_boundnetdevice != 0, 
                 "NanoPU doesn't have a NetDevice to send the packet to!"); 

  return m_boundnetdevice->Send (p->Copy(), 
                                 m_boundnetdevice->GetBroadcast (), 0x0800);
}

bool NanoPuArcht::Send (Ptr<Packet> msg)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << msg);
    
  int txMsgId = m_packetize->ProcessNewMessage (msg);
    
  NanoPuAppHeader apphdr;
  msg->RemoveHeader(apphdr);
    
  if (txMsgId >= 0 )
  {
    m_msgBeginTrace(msg, m_localIp, apphdr.GetRemoteIp(), 
                    apphdr.GetLocalPort(), apphdr.GetRemotePort(), 
                    txMsgId);
    return true;
  }
    
  return false;
}
    
void NanoPuArcht::NotifyApplications (Ptr<Packet> msg, int txMsgId)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << msg);
  if (!m_reassembledMsgCb.IsNull ())
    {
      m_reassembledMsgCb (msg);
    }
  else
    {
      NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
                    " ERROR: NanoPU received a message but"
                    " no application is looking for it. " << 
                    this << " " << msg);
    }
    
  NanoPuAppHeader apphdr;
  msg->RemoveHeader(apphdr);
  m_msgFinishTrace(msg, apphdr.GetRemoteIp(), m_localIp, 
                   apphdr.GetRemotePort(), apphdr.GetLocalPort(), 
                   txMsgId);
}
    
} // namespace ns3