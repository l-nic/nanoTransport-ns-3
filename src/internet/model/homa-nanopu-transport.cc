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
  egressMeta.rank = 0; // High Rank for control packets
    
  if (ctrlMeta.shouldGenCtrlPkt)
  {
    egressMeta.remoteIp = ctrlMeta.remoteIp;
      
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
      
    homah.SetFlags(HomaHeader::Flags_t::BOGUS);
    homah.SetPrio (ctrlMeta.priority);
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
    
std::tuple<uint16_t, uint8_t> 
HomaNanoPuArchtIngressPipe::GetNextMsgIdToGrant (uint16_t mostRecentRxMsgId)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () 
                   << this << mostRecentRxMsgId);
    
  // TODO: Implement an extern that chooses the next msg
  //       to GRANT and determine its priority.
    
  return std::make_tuple(mostRecentRxMsgId, m_nanoPuArcht->GetNumTotalPrioBands ()-1);
}
    
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
    Ipv4Address srcIp = iph.GetSource ();
    uint16_t srcPort = homah.GetSrcPort ();
    uint16_t dstPort = homah.GetDstPort ();
      
    rxMsgInfoMeta_t rxMsgInfo = m_nanoPuArcht->GetReassemblyBuffer ()
                                             ->GetRxMsgInfo (srcIp, 
                                                             srcPort, 
                                                             txMsgId,
                                                             msgLen, 
                                                             pktOffset);
    if (!rxMsgInfo.success)
      return false;
      
    if (rxFlag & HomaHeader::Flags_t::BUSY)
    {
      m_busyMsgs[rxMsgInfo.rxMsgId] = true;
      return true;
    }
    m_busyMsgs[rxMsgInfo.rxMsgId] = false;
      
    if (!rxMsgInfo.isNewPkt)
        return true;
      
    reassembleMeta_t metaData = {};
    metaData.rxMsgId = rxMsgInfo.rxMsgId;
    metaData.srcIp = srcIp;
    metaData.srcPort = srcPort;
    metaData.dstPort = dstPort;
    metaData.txMsgId = txMsgId;
    metaData.msgLen = msgLen;
    metaData.pktOffset = pktOffset;

    m_nanoPuArcht->GetReassemblyBuffer ()->ProcessNewPacket (cp, metaData);
//     Simulator::Schedule (NanoSeconds(HOMA_INGRESS_PIPE_DELAY), 
//                          &NanoPuArchtReassemble::ProcessNewPacket, 
//                          m_nanoPuArcht->GetReassemblyBuffer (),
//                          cp, metaData);
      
    uint16_t ackNo = rxMsgInfo.ackNo;
    if (rxMsgInfo.ackNo == pktOffset)
      ackNo++;
      
    if (ackNo == msgLen)
    {
      homaNanoPuCtrlMeta_t ctrlMeta = {};
      ctrlMeta.shouldUpdateState = false;
      ctrlMeta.shouldGenCtrlPkt = true;
      ctrlMeta.flag = HomaHeader::Flags_t::ACK;
      ctrlMeta.remoteIp = srcIp;
      ctrlMeta.remotePort = srcPort;
      ctrlMeta.localPort = dstPort;
      ctrlMeta.txMsgId = txMsgId;
      ctrlMeta.msgLen = msgLen;
      ctrlMeta.pktOffset = msgLen;
      ctrlMeta.grantOffset = msgLen;
      ctrlMeta.priority = 0;
        
      m_nanoPuArcht->GetPktGen () ->CtrlPktEvent (ctrlMeta);
        
      // Clear the state of this message for simulation performance
      m_busyMsgs.erase(rxMsgInfo.rxMsgId);
      m_pendingMsgInfo.erase(rxMsgInfo.rxMsgId);
        
      return true;
    }
        
    // Update the state of the current msg
    if (rxMsgInfo.isNewMsg)
    {        
      homaNanoPuPendingMsg_t pendingInfo = {};
      pendingInfo.remoteIp = srcIp;
      pendingInfo.remotePort = srcPort;
      pendingInfo.localPort = dstPort;
      pendingInfo.txMsgId = txMsgId;
      pendingInfo.msgLen = msgLen;
      pendingInfo.ackNo = ackNo;
      pendingInfo.grantedIdx = m_nanoPuArcht->GetInitialCredit ();
      pendingInfo.grantableIdx = m_nanoPuArcht->GetInitialCredit () + 1;
      pendingInfo.remainingSize = msgLen -1;
        
      m_pendingMsgInfo[rxMsgInfo.rxMsgId] = pendingInfo;
    }
    else
    {
      m_pendingMsgInfo[rxMsgInfo.rxMsgId].grantableIdx++;
      m_pendingMsgInfo[rxMsgInfo.rxMsgId].remainingSize--;
      m_pendingMsgInfo[rxMsgInfo.rxMsgId].ackNo = ackNo;
    }
      
    // Choose a msg to GRANT and fire CtrlPktEvent with given priority
    uint16_t rxMsgIdToGrant;
    uint8_t priority;
    std::tie(rxMsgIdToGrant, priority) = this->GetNextMsgIdToGrant (rxMsgInfo.rxMsgId);
      
    homaNanoPuCtrlMeta_t ctrlMeta = {};
    ctrlMeta.shouldUpdateState = false;
    ctrlMeta.shouldGenCtrlPkt = true;
    ctrlMeta.flag = HomaHeader::Flags_t::GRANT;
    ctrlMeta.remoteIp = m_pendingMsgInfo[rxMsgIdToGrant].remoteIp;
    ctrlMeta.remotePort = m_pendingMsgInfo[rxMsgIdToGrant].remotePort;
    ctrlMeta.localPort = m_pendingMsgInfo[rxMsgIdToGrant].localPort;
    ctrlMeta.txMsgId = m_pendingMsgInfo[rxMsgIdToGrant].txMsgId;
    ctrlMeta.msgLen = m_pendingMsgInfo[rxMsgIdToGrant].msgLen;
    ctrlMeta.pktOffset = m_pendingMsgInfo[rxMsgIdToGrant].ackNo;
    ctrlMeta.grantOffset = m_pendingMsgInfo[rxMsgIdToGrant].grantableIdx;
    ctrlMeta.priority = priority;
        
    m_nanoPuArcht->GetPktGen () ->CtrlPktEvent (ctrlMeta);
        
    // TODO: Homa keeps a timer per granted inbound messages and send
    //       a RESEND packet once the timer expires.
 
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
                   ->DeliveredEvent (txMsgId, msgLen, setBitMapUntil(pktOffset));
        
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
        
      // Generate a BUSY packet anyway. This packet will be dropped 
      // in the egress pipeline if the message is the active one.
      ctrlMeta.shouldGenCtrlPkt = true;
      ctrlMeta.flag = HomaHeader::Flags_t::BUSY;
      ctrlMeta.remoteIp = iph.GetSource ();
      ctrlMeta.remotePort = homah.GetDstPort ();
      ctrlMeta.localPort = homah.GetSrcPort ();
      ctrlMeta.msgLen = msgLen;
      ctrlMeta.pktOffset = pktOffset;
      ctrlMeta.grantOffset = credit;
        
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
    
uint8_t HomaNanoPuArchtEgressPipe::GetPriority (uint16_t msgLen)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  // TODO: Homa determines the priority of unscheduled packets
  //       based on the msgLen distribution

  if (msgLen <= m_nanoPuArcht->GetInitialCredit ())
    return 0;
  else
    return m_nanoPuArcht->GetNumUnschedPrioBands () -1;
}
    
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
      m_priorities[meta.txMsgId] = this->GetPriority (meta.msgLen);
      
    homah.SetSrcPort (meta.localPort);
    homah.SetDstPort (meta.remotePort);
    homah.SetTxMsgId (meta.txMsgId);
    homah.SetMsgSize (meta.msgLen);
    homah.SetPktOffset (meta.pktOffset);
    homah.SetFlags (HomaHeader::Flags_t::DATA);
    homah.SetPayloadSize ((uint16_t) cp->GetSize ());
    // TODO: Set generation information on the packet. (Not essential)
      
    // Priority of Data packets are determined by the packet tags
    priority = m_priorities[meta.txMsgId];
      
    cp-> AddHeader (homah);
      
    m_activeOutboundMsgId = meta.txMsgId;
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
      m_priorities[meta.txMsgId] = homah.GetPrio();
        
      if (ctrlFlag & HomaHeader::Flags_t::BOGUS)
        return;
    }
      
    if (ctrlFlag & HomaHeader::Flags_t::BUSY &&
        m_activeOutboundMsgId == meta.txMsgId)
    {
      NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                   " NanoPU Homa EgressPipe dropping the BUSY packet"
                   " because it belongs to the most recent (active) msg.");
      return;
    }
      
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU Homa EgressPipe processing control packet.");
    priority = 0; // Highest Priority
  }
    
  Ipv4Header iph;
  iph.SetSource (m_nanoPuArcht->GetLocalIp ());
  iph.SetDestination (meta.remoteIp);
  iph.SetPayloadSize (cp->GetSize ());
  iph.SetTtl (64);
  iph.SetProtocol (HomaHeader::PROT_NUMBER);
  iph.SetTos (priority);
  cp-> AddHeader (iph);
    
  SocketIpTosTag priorityTag;
  priorityTag.SetTos(priority);
  cp-> AddPacketTag (priorityTag);
  NS_LOG_LOGIC("Adding priority tag (" << (uint32_t)priorityTag.GetTos () << 
               ") on the packet where intended priority is " << (uint32_t)priority);
    
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
    .AddAttribute ("EnableArbiterQueueing", 
                   "Enables priority queuing on Arbiter.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&HomaNanoPuArcht::m_enableArbiterQueueing),
                   MakeBooleanChecker ())
    .AddAttribute ("NumTotalPrioBands", "Total number of priority levels used within the network",
                   UintegerValue (8),
                   MakeUintegerAccessor (&HomaNanoPuArcht::m_numTotalPrioBands),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("NumUnschedPrioBands", "Number of priority bands dedicated for unscheduled packets",
                   UintegerValue (2),
                   MakeUintegerAccessor (&HomaNanoPuArcht::m_numUnschedPrioBands),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("OvercommitLevel", "Minimum number of messages to Grant at the same time",
                   UintegerValue (6),
                   MakeUintegerAccessor (&HomaNanoPuArcht::m_overcommitLevel),
                   MakeUintegerChecker<uint8_t> ())
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
    .AddTraceSource ("PacketsInArbiterQueue",
                     "Number of packets currently stored in the arbiter queue",
                     MakeTraceSourceAccessor (&HomaNanoPuArcht::m_nArbiterPackets),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("BytesInArbiterQueue",
                     "Number of bytes (without metadata) currently stored in the arbiter queue",
                     MakeTraceSourceAccessor (&HomaNanoPuArcht::m_nArbiterBytes),
                     "ns3::TracedValueCallback::Uint32")
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
    
uint8_t
HomaNanoPuArcht::GetNumTotalPrioBands (void) const
{
  return m_numTotalPrioBands;
}
    
uint8_t
HomaNanoPuArcht::GetNumUnschedPrioBands (void) const
{
  return m_numUnschedPrioBands;
}
    
uint8_t
HomaNanoPuArcht::GetOvercommitLevel (void) const
{
  return m_overcommitLevel;
}
    
bool HomaNanoPuArcht::EnterIngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                                       uint16_t protocol, const Address &from)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
    
  m_ingresspipe->IngressPipe (device, p, protocol, from);
    
  return true;
}
    
} // namespace ns3