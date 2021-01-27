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
#include <functional>

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"

#include "ns3/node.h"
#include "ns3/ipv4.h"
#include "ns3/data-rate.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/nanopu-archt.h"
#include "homa-nanopu-transport.h"
#include "ns3/ipv4-header.h"
#include "ns3/homa-header.h"

namespace ns3 {
    
NS_LOG_COMPONENT_DEFINE ("HomaNanoPuArcht");

NS_OBJECT_ENSURE_REGISTERED (HomaNanoPuArcht);

/******************************************************************************/

TypeId HomaNanoPuArchtPktGen::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaNanoPuArchtPktGen")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HomaNanoPuArchtPktGen::HomaNanoPuArchtPktGen (Ptr<HomaNanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
}

HomaNanoPuArchtPktGen::~HomaNanoPuArchtPktGen ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HomaNanoPuArchtPktGen::CtrlPktEvent (homaNanoPuCtrlMeta_t ctrlMeta)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU Homa PktGen processing CtrlPktEvent." <<
               " ShouldUpdateState: " << ctrlMeta.shouldUpdateState <<
               " ShouldGenCtrlPkt: " << ctrlMeta.shouldGenCtrlPkt <<
               " (" << HomaHeader::FlagsToString (ctrlMeta.flag) << ")");
  
  Ptr<Packet> p = Create<Packet> ();
  HomaHeader homah;
    
  egressMeta_t egressMeta = {};
  egressMeta.containsData = false;
    
  if (ctrlMeta.shouldGenCtrlPkt)
  {
    egressMeta.dstIP = ctrlMeta.remoteIp;
      
    homah.SetSrcPort (ctrlMeta.localPort);
    homah.SetDstPort (ctrlMeta.remotePort);
    homah.SetTxMsgId (ctrlMeta.txMsgId);
    homah.SetMsgSize (ctrlMeta.msgLen);
    homah.SetPktOffset (ctrlMeta.pktOffset);
    homah.SetGrantOffset (ctrlMeta.grantOffset);
    homah.SetPrio (ctrlMeta.priority);
    homah.SetPayloadSize (0);
    homah.SetFlags (ctrlMeta.flag);
  }
    
  if (ctrlMeta.shouldUpdateState)
  {
    egressMeta.txMsgId = ctrlMeta.txMsgId;
    egressMeta.priority = ctrlMeta.priority;
      
    homah.SetFlags(HomaHeader::Flags_t::BOGUS);
  }
    
  p-> AddHeader (homah);
  m_nanoPuArcht->GetArbiter ()->Receive(p, egressMeta);
}

/******************************************************************************/
 
TypeId HomaNanoPuArchtIngressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaNanoPuArchtIngressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HomaNanoPuArchtIngressPipe::HomaNanoPuArchtIngressPipe (Ptr<HomaNanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
}

HomaNanoPuArchtIngressPipe::~HomaNanoPuArchtIngressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
// uint8_t HomaNanoPuArchtIngressPipe::GetPriority (uint16_t msgLen)
// {
//   NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  
//   uint8_t prio = 0;
//   for (auto & threshold : m_priorities)
//   {
//     if (msgLen <= threshold)
//       return prio;
      
//     prio++;
//   }
//   return prio;
// }
    
bool HomaNanoPuArchtIngressPipe::IngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                                             uint16_t protocol, const Address &from)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU Homa IngressPipe received: " << 
                cp->ToString ());
  
  NS_ASSERT_MSG (protocol==0x0800,
                 "HomaNanoPuArcht works only with IPv4 packets!");
    
  Ipv4Header iph;
  cp->RemoveHeader (iph); 
    
  NS_ASSERT_MSG(iph.GetProtocol() == HomaHeader::PROT_NUMBER,
                "This ingress pipeline only works for Homa Transport");
    
  HomaHeader homah;
  cp->RemoveHeader (homah);
    
  uint16_t txMsgId = homah.GetTxMsgId ();
  uint16_t pktOffset = homah.GetPktOffset ();
  uint16_t msgLen = (uint16_t)homah.GetMsgSize ();
  uint8_t rxFlag = homah.GetFlags ();
    
  if (rxFlag & HomaHeader::Flags_t::DATA ||
      rxFlag & HomaHeader::Flags_t::BUSY )
  {   

//     Ipv4Address srcIp = iph.GetSource ();
//     uint16_t srcPort = homah.GetSrcPort ();
//     uint16_t dstPort = homah.GetDstPort ();
      
//     rxMsgInfoMeta_t rxMsgInfo = m_nanoPuArcht->GetReassemblyBuffer ()
//                                              ->GetRxMsgInfo (srcIp, 
//                                                              srcPort, 
//                                                              txMsgId,
//                                                              msgLen, 
//                                                              pktOffset);
      
//     // NOTE: The ackNo in the rxMsgInfo is the acknowledgement number
//     //       before processing this incoming data packet because this
//     //       packet has not updated the receivedBitmap in the reassembly
//     //       buffer yet.
      
//     uint8_t responseFlag = 0;
//     uint16_t grantOffsetDiff;
//     if (rxFlag & HomaHeader::Flags_t::RESEND)
//     {
//       NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
//                    " NanoPU Homa IngressPipe processing RESEND request.");
//       grantOffsetDiff = 0;
//       if (rxMsgInfo.isNewPkt)
//         responseFlag |= HomaHeader::Flags_t::RSNDRSPNS;
//     } 
//     else 
//     {
//       NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
//                    " NanoPU Homa IngressPipe processing DATA packet.");
//       grantOffsetDiff = 1;
//     }
      
//     // Compute grantOffset with a PRAW extern
//     uint16_t grantOffset = 0;
//     if (rxMsgInfo.isNewMsg)
//     {
//       m_credits[rxMsgInfo.rxMsgId] = m_nanoPuArcht->GetInitialCredit ()
//                                      + grantOffsetDiff;
//     }
//     else
//     {
//       m_credits[rxMsgInfo.rxMsgId] += grantOffsetDiff;
//     }
//     grantOffset = m_credits[rxMsgInfo.rxMsgId];
      
//     // Compute the priority of the message and find the active msg
//     uint8_t priority = GetPriority (msgLen);
      
//     // Begin Read-Modify-(Delete/Write) Operation
//     bool scheduledMsgsIsEmpty = m_scheduledMsgs[priority].empty();
//     uint16_t activeRxMsgId = m_scheduledMsgs[priority].front();
      
//     if (scheduledMsgsIsEmpty || activeRxMsgId==rxMsgInfo.rxMsgId)
//     {
//       // Msg of the received pkt is the active one for this prio
//       responseFlag |= HomaHeader::Flags_t::GRANT;
       
//       m_nanoPuArcht->GetPktGen ()
//                    ->CtrlPktEvent(responseFlag, srcIp, srcPort, 
//                                   dstPort, txMsgId, msgLen, 
//                                   pktOffset, grantOffset, priority);
        
//       if (!scheduledMsgsIsEmpty && grantOffset >= msgLen)
//         // The active msg is fully granted, so unschedule it. 
//         // BUSY packets will be used to ACK remaining packets.
// //       if (!scheduledMsgsIsEmpty 
// //           && rxMsgInfo.numPkts == msgLen-1
// //           && rxMsgInfo.ackNo == pktOffset)
//       {
//         // This was the last expected packet of the message
//         m_scheduledMsgs[priority].pop_front();
//         // TODO: Activate the next message and send a grant.
//         //       Otherwise the sender should retransmit to get Granted.
//       }
//     }
//     else
//     {
//       // This packet doesn't belong to an active msg
//       responseFlag |= HomaHeader::Flags_t::BUSY;
        
//       m_nanoPuArcht->GetPktGen ()
//                    ->CtrlPktEvent(responseFlag, srcIp, srcPort, 
//                                   dstPort, txMsgId, msgLen, 
//                                   pktOffset, grantOffset, priority);
//     }
      
//     if ((scheduledMsgsIsEmpty ||  rxMsgInfo.isNewMsg) && grantOffset < msgLen)
//     {
//       m_scheduledMsgs[priority].push_back(rxMsgInfo.rxMsgId);
//     }
//     // End Read-Modify-(Delete/Write) Operation
      
//     if (rxFlag & HomaHeader::Flags_t::DATA)
//     {
//       reassembleMeta_t metaData;
//       metaData.rxMsgId = rxMsgInfo.rxMsgId;
//       metaData.srcIp = srcIp;
//       metaData.srcPort = srcPort;
//       metaData.dstPort = dstPort;
//       metaData.txMsgId = txMsgId;
//       metaData.msgLen = msgLen;
//       metaData.pktOffset = pktOffset;
            
//       m_nanoPuArcht->GetReassemblyBuffer ()->ProcessNewPacket (cp, metaData);
// //       Simulator::Schedule (NanoSeconds(HOMA_INGRESS_PIPE_DELAY), 
// //                            &NanoPuArchtReassemble::ProcessNewPacket, 
// //                            m_nanoPuArcht->GetReassemblyBuffer (),
// //                            cp, metaData);
//     }
      
    // TODO: Deactivate the current msg (should be keeping a list of active msgs)
 
  }  
  else if (rxFlag & HomaHeader::Flags_t::GRANT ||
           rxFlag & HomaHeader::Flags_t::RESEND ||
           rxFlag & HomaHeader::Flags_t::ACK)
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU Homa IngressPipe processing a "
                 << homah.FlagsToString(rxFlag) << " packet.");
      
    int rtxPkt = -1;
    int credit = homah.GetGrantOffset ();
      
    homaNanoPuCtrlMeta_t ctrlMeta = {};
    ctrlMeta.txMsgId = txMsgId;
    ctrlMeta.priority = homah.GetPrio ();
    ctrlMeta.shouldUpdateState = true;
    
    if (rxFlag & HomaHeader::Flags_t::GRANT)
    {
      m_nanoPuArcht->GetPacketizationBuffer ()
                   ->DeliveredEvent (txMsgId, msgLen, (((bitmap_t)1)<<pktOffset));
        
      m_nanoPuArcht->GetPacketizationBuffer ()
                   ->CreditToBtxEvent (txMsgId, rtxPkt, credit, credit,
                                       NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
                                       std::greater<int>());
      
      m_nanoPuArcht->GetPktGen () ->CtrlPktEvent (ctrlMeta);
    }
    else if (rxFlag & HomaHeader::Flags_t::RESEND)
    {   
      rtxPkt = pktOffset;
      m_nanoPuArcht->GetPacketizationBuffer ()
                   ->CreditToBtxEvent (txMsgId, rtxPkt, credit, credit,
                                       NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
                                       std::greater<int>());
        
      // TODO: The receiver might be sending a RESEND but the sender might 
      //       be busy with other msgs out, so the sender sends out a BUSY packet.
      // if (!TargetMsgIsTheHighestPrioOne())
      // {
      //   ctrlMeta.shouldGenCtrlPkt = true;
      //   ctrlMeta.flag = HomaHeader::Flags_t::BUSY;
      //   ctrlMeta.remoteIp = iph.GetSource ();
      //   ctrlMeta.remotePort = homah.GetDstPort ();
      //   ctrlMeta.localPort = homah.GetSrcPort ();
      //   ctrlMeta.msgLen = msgLen;
      //   ctrlMeta.pktOffset = pktOffset;
      //   ctrlMeta.grantOffset = credit;
      // }
      // TODO: Should keep a list of active msgs for tx direction as well.
      m_nanoPuArcht->GetPktGen () ->CtrlPktEvent (ctrlMeta);
    }
    else if (rxFlag & HomaHeader::Flags_t::ACK)
    {
      m_nanoPuArcht->GetPacketizationBuffer ()
                   ->DeliveredEvent (txMsgId, msgLen, setBitMapUntil(msgLen));
    }
  }
  else
  {
    NS_LOG_WARN (Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU Homa IngressPipe received an unknown ("
                 << homah.FlagsToString(rxFlag) << ") type of a packet.");
  }
    
  return true;
}
    
/******************************************************************************/
    
TypeId HomaNanoPuArchtEgressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaNanoPuArchtEgressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HomaNanoPuArchtEgressPipe::HomaNanoPuArchtEgressPipe (Ptr<HomaNanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << nanoPuArcht);
    
  m_nanoPuArcht = nanoPuArcht;
}

HomaNanoPuArchtEgressPipe::~HomaNanoPuArchtEgressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
// uint8_t HomaNanoPuArchtEgressPipe::GetPriority (uint16_t msgLen)
// {
//   NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  
//   uint8_t prio = 0;
//   for (auto & threshold : m_priorityCutoffs)
//   {
//     if (msgLen <= threshold)
//       return prio;
      
//     prio++;
//   }
//   return prio;
// }
    
void HomaNanoPuArchtEgressPipe::EgressPipe (Ptr<const Packet> p, egressMeta_t meta)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
    
  HomaHeader homah;
  uint8_t priority;
  
  if (meta.containsData)
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU Homa EgressPipe processing data packet.");
      
    if (meta.isNewMsg)
      m_priorities[meta.txMsgId] = 0;
      
    homah.SetSrcPort (meta.srcPort);
    homah.SetDstPort (meta.dstPort);
    homah.SetTxMsgId (meta.txMsgId);
    homah.SetMsgSize (meta.msgLen);
    homah.SetPktOffset (meta.pktOffset);
    homah.SetFlags (HomaHeader::Flags_t::DATA);
    homah.SetPayloadSize ((uint16_t) cp->GetSize ());
      
    // Priority of Data packets are determined by the packet tags
    priority = m_priorities[meta.txMsgId];
    
    // TODO: Set generation information on the packet. (Not essential)
      
    cp-> AddHeader (homah);
  }
  else
  {
    cp-> PeekHeader (homah);
    uint8_t ctrlFlag = homah.GetFlags ();
    if (ctrlFlag & HomaHeader::Flags_t::BOGUS ||
        ctrlFlag & HomaHeader::Flags_t::BUSY)
    {
      NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                   " NanoPU Homa EgressPipe updating local state.");
      m_priorities[meta.txMsgId] = meta.priority;
        
      if (ctrlFlag & HomaHeader::Flags_t::BOGUS)
        return;
    }
      
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU Homa EgressPipe processing control packet.");
    priority = 0; // Highest Priority
  }
    
  Ipv4Header iph;
  iph.SetSource (m_nanoPuArcht->GetLocalIp ());
  iph.SetDestination (meta.dstIP);
  iph.SetPayloadSize (cp->GetSize ());
  iph.SetTtl (64);
  iph.SetProtocol (HomaHeader::PROT_NUMBER);
  iph.SetTos (priority);
  cp-> AddHeader (iph);
    
  SocketIpTosTag priorityTag;
  priorityTag.SetTos(priority);
  cp-> AddPacketTag (priorityTag);
  NS_LOG_LOGIC("Adding priority tag on the packet: " << 
               (uint32_t)priorityTag.GetTos () << 
               " where intended priority is " << (uint32_t)priority);
    
  NS_ASSERT_MSG(cp->PeekPacketTag (priorityTag),
                "The packet should have a priority tag before transmission!");
  
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU Homa EgressPipe sending: " << 
                cp->ToString ());
    
  m_nanoPuArcht->SendToNetwork(cp);
//   Simulator::Schedule (NanoSeconds(HOMA_EGRESS_PIPE_DELAY), 
//                        &NanoPuArcht::SendToNetwork, m_nanoPuArcht, cp);

  return;
}
    
/******************************************************************************/
       
TypeId HomaNanoPuArcht::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaNanoPuArcht")
    .SetParent<Object> ()
    .SetGroupName("Network")
    .AddConstructor<HomaNanoPuArcht> ()
    .AddAttribute ("PayloadSize", 
                   "MTU for the network interface excluding the header sizes",
                   UintegerValue (1400),
                   MakeUintegerAccessor (&HomaNanoPuArcht::m_payloadSize),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxNMessages", 
                   "Maximum number of messages NanoPU can handle at a time",
                   UintegerValue (100),
                   MakeUintegerAccessor (&HomaNanoPuArcht::m_maxNMessages),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TimeoutInterval", "Time value to expire the timers",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&HomaNanoPuArcht::m_timeoutInterval),
                   MakeTimeChecker (MicroSeconds (0)))
    .AddAttribute ("InitialCredit", "Initial window of packets to be sent",
                   UintegerValue (10),
                   MakeUintegerAccessor (&HomaNanoPuArcht::m_initialCredit),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxNTimeouts", 
                   "Max allowed number of retransmissions before discarding a msg",
                   UintegerValue (5),
                   MakeUintegerAccessor (&HomaNanoPuArcht::m_maxTimeoutCnt),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("OptimizeMemory", 
                   "High performant mode (only packet sizes are stored to save from memory).",
                   BooleanValue (true),
                   MakeBooleanAccessor (&HomaNanoPuArcht::m_memIsOptimized),
                   MakeBooleanChecker ())
    .AddTraceSource ("MsgBegin",
                     "Trace source indicating a message has been delivered to "
                     "the the NanoPuArcht by the sender application layer.",
                     MakeTraceSourceAccessor (&HomaNanoPuArcht::m_msgBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MsgFinish",
                     "Trace source indicating a message has been delivered to "
                     "the receiver application by the NanoPuArcht layer.",
                     MakeTraceSourceAccessor (&HomaNanoPuArcht::m_msgFinishTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}

HomaNanoPuArcht::HomaNanoPuArcht () : NanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}

HomaNanoPuArcht::~HomaNanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HomaNanoPuArcht::AggregateIntoDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << device); 
    
  NanoPuArcht::AggregateIntoDevice (device);
    
  m_pktgen = CreateObject<HomaNanoPuArchtPktGen> (this);
    
  m_egresspipe = CreateObject<HomaNanoPuArchtEgressPipe> (this);
    
  m_arbiter->SetEgressPipe (m_egresspipe);
    
  m_ingresspipe = CreateObject<HomaNanoPuArchtIngressPipe> (this);
}
    
Ptr<HomaNanoPuArchtPktGen> 
HomaNanoPuArcht::GetPktGen (void)
{
  return m_pktgen;
}
    
bool HomaNanoPuArcht::EnterIngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                                       uint16_t protocol, const Address &from)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
    
  m_ingresspipe->IngressPipe (device, p, protocol, from);
    
  return true;
}
    
} // namespace ns3