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
#include "node.h"
#include "ns3/ipv4.h"
#include "nanopu-archt.h"
#include "ns3/ipv4-header.h"
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
  bitmap_t result;
  for (uint16_t i=0; i<n; i++){
    result.set(i);
  }
  return result;
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
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
  NS_LOG_ERROR (Simulator::Now ().GetNanoSeconds () <<
                "ERROR: NanoPU wants to send a packet of size " << 
                p->GetSize () << ", but te egress pipe can not be found!");
    
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

NanoPuArchtArbiter::NanoPuArchtArbiter ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
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
    
  // TODO: The arbiter acts as a priority queue that pulls packets 
  //       from the packetization buffer and the packet generator.
  //       Then the arbiter should prioritize control packets over
  //       data packets for higher performance.
    
//   m_egressPipe->EgressPipe(p, meta);
  Simulator::Schedule (NanoSeconds(PACKETIZATION_DELAY),
                       &NanoPuArchtEgressPipe::EgressPipe, 
                       m_egressPipe, p, meta);
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

NanoPuArchtPacketize::NanoPuArchtPacketize (Ptr<NanoPuArchtArbiter> arbiter,
                                            uint16_t maxMessages,
                                            uint16_t initialCredit,
                                            uint16_t payloadSize,
                                            uint16_t maxTimeoutCnt)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_arbiter = arbiter;
    
  m_txMsgIdFreeList.resize(maxMessages);
  std::iota(m_txMsgIdFreeList.begin(), m_txMsgIdFreeList.end(), 0);
    
  m_initialCredit = initialCredit;
  m_payloadSize = payloadSize;
  m_maxTimeoutCnt = maxTimeoutCnt;
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
               " NanoPU DeliveredEvent for msg " << txMsgId <<
               " packets (bitmap) " << std::bitset<BITMAP_SIZE>(ackPkts) );
    
  if (m_deliveredBitmap.find(txMsgId) != m_deliveredBitmap.end() \
      && std::find(m_txMsgIdFreeList.begin(),m_txMsgIdFreeList.end(),txMsgId) == m_txMsgIdFreeList.end())
  {
    m_deliveredBitmap[txMsgId] |= ackPkts;
      
//     if (m_deliveredBitmap[txMsgId] == (((bitmap_t)1)<<msgLen)-1)
    if (m_deliveredBitmap[txMsgId] == setBitMapUntil(msgLen))
    {
      NS_LOG_LOGIC("The whole message is delivered.");
        
      m_timer->CancelTimerEvent (txMsgId);
        
      /* Free the txMsgId*/
      m_txMsgIdFreeList.push_back (txMsgId);
    }
  }
  else
  {
    NS_LOG_ERROR(Simulator::Now ().GetNanoSeconds () <<
                 " ERROR: DeliveredEvent was triggered for unknown tx_msg_id: "
                 << txMsgId);
  }
}
    
void NanoPuArchtPacketize::CreditToBtxEvent (uint16_t txMsgId, int rtxPkt, 
                                             int newCredit, int compVal, 
                                             CreditEventOpCode_t opCode, 
                                             std::function<bool(int,int)> relOp)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () << 
               " NanoPU CreditToBtxEvent for msg " << txMsgId );
    
  if (std::find(m_txMsgIdFreeList.begin(),m_txMsgIdFreeList.end(),txMsgId) == m_txMsgIdFreeList.end())
  {
    if (rtxPkt != -1 && m_deliveredBitmap.find(txMsgId) != m_deliveredBitmap.end())
    {
      NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () <<
                   " Marking msg " << txMsgId << ", pkt " << rtxPkt <<
                   " for retransmission.");
      m_toBeTxBitmap[txMsgId] |= (((bitmap_t)1)<<rtxPkt);
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
          
        NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () <<
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
        Dequeue (txMsgId, txPkts, false);
        m_toBeTxBitmap[txMsgId] &= ~txPkts;
      }
    }
  }
  else
  {
    NS_LOG_ERROR(Simulator::Now ().GetNanoSeconds () <<
                 " ERROR: CreditToBtxEvent was triggered for unknown tx_msg_id: "
                 << txMsgId);
  }
}
    
void NanoPuArchtPacketize::TimeoutEvent (uint16_t txMsgId, uint16_t rtxOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () << 
               " NanoPU Timeout for msg " << txMsgId << " in Packetization Buffer");
    
  if (std::find(m_txMsgIdFreeList.begin(),m_txMsgIdFreeList.end(),txMsgId) == m_txMsgIdFreeList.end())
  {
    if (m_timeoutCnt[txMsgId] >= m_maxTimeoutCnt)
    {
      NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
                   " Msg " << txMsgId << " expired.");
        
      /* Free the txMsgId*/
      m_txMsgIdFreeList.push_back (txMsgId);
    }
    else
    {
      m_timeoutCnt[txMsgId] ++;
//       bitmap_t rtxPkts = (~m_deliveredBitmap[txMsgId]) & ((((bitmap_t)1)<<(rtxOffset+1))-1);
      bitmap_t rtxPkts = (~m_deliveredBitmap[txMsgId]) & setBitMapUntil(rtxOffset+1);
      
      if (rtxPkts.any()) 
      { 
        NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () <<
                   " NanoPU will retransmit " << std::bitset<BITMAP_SIZE>(rtxPkts) );
        
        Dequeue (txMsgId, rtxPkts, true);
        m_toBeTxBitmap[txMsgId] &= ~rtxPkts;
      }
        
      m_timer->RescheduleTimerEvent (txMsgId, m_maxTxPktOffset[txMsgId]);
    }
  }
  else
  {
    NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
                 " TimeoutEvent was triggered for unknown tx_msg_id: "
                 << txMsgId);
  }
}
    
bool NanoPuArchtPacketize::ProcessNewMessage (Ptr<Packet> msg)
{
  Ptr<Packet> cmsg = msg->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cmsg);
    
  NanoPuAppHeader apphdr;
  cmsg->RemoveHeader (apphdr);
    
  NS_ASSERT_MSG (apphdr.GetHeaderType() == NANOPU_APP_HEADER_TYPE, 
                 "NanoPU expects packets to have NanoPU App Header!");
    
  NS_ASSERT_MSG (apphdr.GetPayloadSize()/ m_payloadSize < BITMAP_SIZE,
                 "NanoPU can not handle messages larger than "<<BITMAP_SIZE<<" packets!");
  
  uint16_t txMsgId;
  if (m_txMsgIdFreeList.size() > 0)
  {
    txMsgId = m_txMsgIdFreeList.front ();
    NS_LOG_LOGIC("NanoPU Packetization Buffer allocating txMsgId: " << txMsgId);
    m_txMsgIdFreeList.pop_front ();
    
    NS_ASSERT_MSG (apphdr.GetPayloadSize() == (uint16_t) cmsg->GetSize (),
                   "The payload size in the NanoPU App header doesn't match real payload size. " <<
                   apphdr.GetPayloadSize() << " vs " << (uint16_t) cmsg->GetSize ());
    m_appHeaders[txMsgId] = apphdr;
     
    std::map<uint16_t,Ptr<Packet>> buffer;
    uint32_t remainingBytes = cmsg->GetSize ();
    uint16_t numPkts = 0;
    uint32_t nextPktSize;
    Ptr<Packet> nextPkt;
    while (remainingBytes > 0)
    {
//       NS_LOG_DEBUG("Remaining bytes to be packetized: "<<remainingBytes<<
//                    " Max payload size: "<<m_payloadSize<<
//                    " Org. Msg Size: "<<cmsg->GetSize ());
      nextPktSize = std::min(remainingBytes, (uint32_t) m_payloadSize);
      nextPkt = cmsg->CreateFragment (cmsg->GetSize () - remainingBytes, nextPktSize);
      buffer[numPkts] = nextPkt;
      remainingBytes -= nextPktSize;
      numPkts ++;
    }
    m_buffers[txMsgId] = buffer;
    NS_ASSERT_MSG (apphdr.GetMsgLen() == numPkts,
                   "The message length in the NanoPU App header doesn't match number of packets for this message.");
      
    uint16_t requestedCredit = apphdr.GetInitWinSize ();  
    m_credits[txMsgId] = (requestedCredit < m_initialCredit) ? requestedCredit : m_initialCredit;
      
    m_deliveredBitmap[txMsgId] = 0;
      
    m_toBeTxBitmap[txMsgId] = setBitMapUntil(numPkts);
      
    m_maxTxPktOffset[txMsgId] = 0;
      
    m_timeoutCnt[txMsgId] = 0;
    
    m_timer->ScheduleTimerEvent (txMsgId, 0);
      
//     bitmap_t txPkts = m_toBeTxBitmap[txMsgId] & ((((bitmap_t)1)<<m_credits[txMsgId])-1);
    bitmap_t txPkts = m_toBeTxBitmap[txMsgId] & setBitMapUntil(m_credits[txMsgId]);
    // TODO: txPkts should be placed in a fifo queue where schedulings
    //       from other events also place packets to. Since this is just
    //       a discrete event simulator, direct function calls would also
    //       work as a fifo.
    Dequeue (txMsgId, txPkts, false);
      
    m_toBeTxBitmap[txMsgId] &= ~txPkts;
    
  }
  else
  {
    NS_LOG_ERROR(Simulator::Now ().GetNanoSeconds () << 
                 " Error: NanoPU could not allocate a new txMsgId for the new message. " << 
                 this << " " << msg);
    return false;
  }
    
  return true;
}
    
void NanoPuArchtPacketize::Dequeue (uint16_t txMsgId, bitmap_t txPkts, bool isRtx)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << txMsgId << txPkts);
  
  egressMeta_t meta;
  uint16_t pktOffset = getFirstSetBitPos(txPkts);
  while (pktOffset != BITMAP_SIZE)
  {
    NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
                 " NanoPU Packetization Buffer transmitting pkt " <<
                 pktOffset << " from msg " << txMsgId);
    
    Ptr<Packet> p = m_buffers[txMsgId][pktOffset];
    
    NanoPuAppHeader apphdr = m_appHeaders[txMsgId];
    meta.isData = true;
    meta.isRtx = isRtx;
    meta.dstIP = apphdr.GetRemoteIp();
    meta.dstPort = apphdr.GetRemotePort();
    meta.srcPort = apphdr.GetLocalPort();
    meta.txMsgId = txMsgId;
    meta.msgLen = apphdr.GetMsgLen();
    meta.pktOffset = pktOffset;
      
    m_arbiter->Receive(p, meta);
      
    txPkts &= ~(((bitmap_t)1)<<pktOffset);
    if (pktOffset > m_maxTxPktOffset[txMsgId])
    {
      m_maxTxPktOffset[txMsgId] = pktOffset;
    }
    
    pktOffset = getFirstSetBitPos(txPkts);
  }
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

NanoPuArchtEgressTimer::NanoPuArchtEgressTimer (Ptr<NanoPuArchtPacketize> packetize,
                                    Time timeoutInterval)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_packetize = packetize;
  m_timeoutInterval = timeoutInterval;
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
  
  m_timers[txMsgId] = Simulator::Schedule (m_timeoutInterval,
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
  else
  {
    NS_LOG_WARN("NanoPU Egress CancelTimer Event for an unknown timer!");
  }
}
    
void NanoPuArchtEgressTimer::RescheduleTimerEvent (uint16_t txMsgId, uint16_t rtxOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " NanoPU Egress RescheduleTimer Event for msg " << txMsgId <<
                " rtxOffset " << rtxOffset);
    
  m_timers[txMsgId] = Simulator::Schedule (m_timeoutInterval,
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

NanoPuArchtReassemble::NanoPuArchtReassemble (uint16_t maxMessages)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_rxMsgIdFreeList.resize(maxMessages);
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
    
void 
NanoPuArchtReassemble::SetRecvCallback (Callback<void, Ptr<Packet> > reassembledMsgCb)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << &reassembledMsgCb);
  NS_ASSERT_MSG(m_reassembledMsgCb.IsNull (),
                "An application has already been installed on this NanoPU!");
  m_reassembledMsgCb = reassembledMsgCb;
}
    
void NanoPuArchtReassemble::NotifyApplications (Ptr<Packet> msg)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << msg);
  if (!m_reassembledMsgCb.IsNull ())
    {
      m_reassembledMsgCb (msg);
    }
  else
    {
      NS_LOG_ERROR (Simulator::Now ().GetNanoSeconds () << 
                    " Error: NanoPU received a message but no application is looking for it. " << 
                    this << " " << msg);
    }
}

rxMsgInfoMeta_t 
NanoPuArchtReassemble::GetRxMsgInfo (Ipv4Address srcIp, uint16_t srcPort, 
                                     uint16_t txMsgId, uint16_t msgLen, 
                                     uint16_t pktOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << 
                   srcIp << srcPort << txMsgId << msgLen << pktOffset);
    
  rxMsgInfoMeta_t rxMsgInfo;
  rxMsgInfo.isNewMsg = false;
  rxMsgInfo.isNewPkt = false;
  rxMsgInfo.success = false;
    
  rxMsgIdTableKey_t key (srcIp.Get (), srcPort, txMsgId);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
                " NanoPU Reassembly Buffer processing GetRxMsgInfo extern call for: " << srcIp.Get () 
                                                                                     << "-" << srcPort
                                                                                     << "-" << txMsgId); 
  auto entry = m_rxMsgIdTable.find(key);
  if (entry != m_rxMsgIdTable.end())
  {
    rxMsgInfo.rxMsgId = entry->second;
    NS_LOG_LOGIC("NanoPU Reassembly Buffer Found rxMsgId: " << entry->second);
      
    // compute the beginning of the inflight window
    rxMsgInfo.ackNo = getFirstSetBitPos(~m_receivedBitmap.find (rxMsgInfo.rxMsgId)->second);
    if (rxMsgInfo.ackNo == BITMAP_SIZE)
    {
      NS_LOG_LOGIC("Msg " << rxMsgInfo.rxMsgId << "has already been fully received");
      rxMsgInfo.ackNo = msgLen;
    }
      
    rxMsgInfo.numPkts = m_buffers.find (rxMsgInfo.rxMsgId) ->second.size();
      
    rxMsgInfo.isNewPkt = (m_receivedBitmap.find (rxMsgInfo.rxMsgId)->second & (((bitmap_t)1)<<pktOffset)) == 0;
    rxMsgInfo.success = true;
  }
  // try to allocate an rx_msg_id
  else if (m_rxMsgIdFreeList.size() > 0)
  {
    rxMsgInfo.rxMsgId = m_rxMsgIdFreeList.front ();
    NS_LOG_LOGIC("NanoPU Reassembly Buffer allocating rxMsgId: " << rxMsgInfo.rxMsgId);
    m_rxMsgIdFreeList.pop_front ();
     
    m_rxMsgIdTable.insert({key,rxMsgInfo.rxMsgId});
    m_receivedBitmap.insert({rxMsgInfo.rxMsgId,0});
    std::map<uint16_t,Ptr<Packet>> buffer;
    m_buffers.insert({rxMsgInfo.rxMsgId,buffer});
    
    rxMsgInfo.ackNo = 0;
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
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
                " Processing pkt "<< meta.pktOffset 
                << " for msg " << meta.rxMsgId);
    
  /* Record pkt in buffer*/
  auto buffer = m_buffers.find (meta.rxMsgId);
  NS_ASSERT (buffer != m_buffers.end ());
  buffer->second [meta.pktOffset] = pkt;
    
  /* Mark the packet as received*/
  // NOTE: received_bitmap must have 2 write ports: here and in getRxMsgInfo()
  m_receivedBitmap [meta.rxMsgId] |= (((bitmap_t)1)<<meta.pktOffset);
    
  /* Check if all pkts have been received*/
//   if (m_receivedBitmap [meta.rxMsgId] == (((bitmap_t)1)<<meta.msgLen)-1)
  if (m_receivedBitmap [meta.rxMsgId] == setBitMapUntil(meta.msgLen))
  {
    NS_LOG_LOGIC ("All packets have been received for msg " << meta.rxMsgId);
      
    /* Push the reassembled msg to the applications*/
    Ptr<Packet> msg = Create<Packet> ();
    for (uint16_t i=0; i<meta.msgLen; i++)
    {
      msg->AddAtEnd (buffer->second[i]);
    }
    NanoPuAppHeader apphdr;
    apphdr.SetRemoteIp (meta.srcIp);
    apphdr.SetRemotePort (meta.srcPort);
    apphdr.SetLocalPort (meta.dstPort);
    apphdr.SetMsgLen (meta.msgLen);
    apphdr.SetPayloadSize (msg->GetSize ());
    msg->AddHeader (apphdr);
    
//     NotifyApplications (msg);
    Simulator::Schedule (NanoSeconds(REASSEMBLE_DELAY), 
                         &NanoPuArchtReassemble::NotifyApplications, 
                         this, msg);
      
    /* Free the rxMsgId*/
    rxMsgIdTableKey_t key (meta.srcIp.Get (), meta.srcPort, meta.txMsgId);
    auto map = m_rxMsgIdTable.find (key);
    NS_ASSERT (meta.rxMsgId == map->second);
    m_rxMsgIdTable.erase (map);
    m_rxMsgIdFreeList.push_back (meta.rxMsgId);
      
    m_timer->CancelTimerEvent (meta.rxMsgId);
  }
  else
  {
    m_timer->ScheduleTimerEvent (meta.rxMsgId);
  }
}
    
void NanoPuArchtReassemble::TimeoutEvent (uint16_t rxMsgId)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () << 
               " NanoPU Timeout for msg " << rxMsgId << " in Reassembly Buffer");
    
  if (std::find(m_rxMsgIdFreeList.begin(),m_rxMsgIdFreeList.end(),rxMsgId) == m_rxMsgIdFreeList.end())
  {
    NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
                 " Msg " << rxMsgId << " expired.");
        
    /* Free the rxMsgId*/
    m_rxMsgIdFreeList.push_back (rxMsgId);
  }
  else
  {
    NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
                 " TimeoutEvent was triggered for unknown rx_msg_id: "
                 << rxMsgId);
  }
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

NanoPuArchtIngressTimer::NanoPuArchtIngressTimer (Ptr<NanoPuArchtReassemble> reassemble,
                                                  Time timeoutInterval)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_reassemble = reassemble;
  m_timeoutInterval = timeoutInterval;
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
  m_timers[rxMsgId] = Simulator::Schedule (m_timeoutInterval,
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
  ;
  return tid;
}

/*
 * NanoPu Architecture requires a transport module.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for constructor
 */
NanoPuArcht::NanoPuArcht (Ptr<Node> node,
                          Ptr<NetDevice> device,
                          Time timeoutInterval,
                          uint16_t maxMessages,
                          uint16_t payloadSize, 
                          uint16_t initialCredit,
                          uint16_t maxTimeoutCnt)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_node = node;
  m_boundnetdevice = device;
  BindToNetDevice ();
    
  m_maxMessages = maxMessages;
  m_payloadSize = payloadSize;
  m_initialCredit = initialCredit;
    
  m_reassemble = CreateObject<NanoPuArchtReassemble> (m_maxMessages);
  m_arbiter = CreateObject<NanoPuArchtArbiter> ();
  m_packetize = CreateObject<NanoPuArchtPacketize> (m_arbiter,
                                                    m_maxMessages,
                                                    m_initialCredit,
                                                    m_payloadSize,
                                                    maxTimeoutCnt);
  m_egressTimer = CreateObject<NanoPuArchtEgressTimer> (m_packetize, timeoutInterval);
  m_packetize->SetTimerModule (m_egressTimer);
    
  m_ingressTimer = CreateObject<NanoPuArchtIngressTimer> (m_reassemble, timeoutInterval*2);
  m_reassemble->SetTimerModule (m_ingressTimer);
}

/*
 * NanoPu Architecture requires a transport module.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for destructor
 */
NanoPuArcht::~NanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
/* Returns associated node */
Ptr<Node>
NanoPuArcht::GetNode (void)
{
  return m_node;
}
    
uint16_t NanoPuArcht::GetPayloadSize (void)
{
  return m_payloadSize;
}

/*
 * NanoPu Architecture requires a transport module. The function below
 * is a reference implementation for future transport modules.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for the real implementation.
 */
void
NanoPuArcht::BindToNetDevice ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << m_boundnetdevice);
    
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
      NS_ASSERT_MSG (found, "NanoPU cannot be bound to a NetDevice not existing on the Node");
    }

  m_boundnetdevice->SetReceiveCallback (MakeCallback (&NanoPuArcht::EnterIngressPipe, this));
  m_mtu = m_boundnetdevice->GetMtu ();
  return;
}

Ptr<NetDevice>
NanoPuArcht::GetBoundNetDevice ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  return m_boundnetdevice;
}
    
Ptr<NanoPuArchtReassemble> 
NanoPuArcht::GetReassemblyBuffer (void)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  return m_reassemble;
}
    
Ptr<NanoPuArchtArbiter> NanoPuArcht::GetArbiter (void)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  return m_arbiter;
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
  NS_LOG_DEBUG ("At time " <<  Simulator::Now ().GetNanoSeconds () << 
               " NanoPU received a packet of size " << p->GetSize () <<
               ", but no transport protocol has been programmed.");
    
  return false;
}
    
bool
NanoPuArcht::SendToNetwork (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
  NS_ASSERT_MSG (m_boundnetdevice != 0, "NanoPU doesn't have a NetDevice to send the packet to!"); 

  return m_boundnetdevice->Send (p, m_boundnetdevice->GetBroadcast (), 0x0800);
}

bool
NanoPuArcht::Send (Ptr<Packet> msg)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << msg);
    
  return m_packetize->ProcessNewMessage (msg->Copy ());
}
    
} // namespace ns3