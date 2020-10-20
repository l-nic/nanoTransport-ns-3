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

HomaNanoPuArchtPktGen::HomaNanoPuArchtPktGen (Ptr<NanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
    
  Ptr<NetDevice> netDevice = m_nanoPuArcht->GetBoundNetDevice ();
  PointToPointNetDevice* p2pNetDevice = dynamic_cast<PointToPointNetDevice*>(&(*(netDevice))); 
  
  DataRate dataRate = p2pNetDevice->GetDataRate ();
  uint16_t mtuBytes = m_nanoPuArcht->GetBoundNetDevice ()->GetMtu ();
  m_packetTxTime = dataRate.CalculateBytesTxTime ((uint32_t) mtuBytes);
    
  // Set an initial value for the last Tx time.
  m_pacerLastTxTime = Simulator::Now () - m_packetTxTime;
}

HomaNanoPuArchtPktGen::~HomaNanoPuArchtPktGen ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HomaNanoPuArchtPktGen::CtrlPktEvent (bool genGRANT, bool genBUSY,
                                         Ipv4Address dstIp, uint16_t dstPort, 
                                         uint16_t srcPort, uint16_t txMsgId, 
                                         uint16_t msgLen, uint16_t pktOffset, 
                                         uint16_t grantOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU Homa PktGen processing CtrlPktEvent." <<
               " GenGrant: " << genGRANT << " GenBUSY: " << genBUSY);
    
  egressMeta_t meta;
  meta.isData = false;
  meta.dstIP = dstIp;
    
  HomaHeader homah;
  homah.SetSrcPort (srcPort);
  homah.SetDstPort (dstPort);
  homah.SetTxMsgId (txMsgId);
  homah.SetMsgLen (msgLen);
  homah.SetPktOffset (pktOffset);
  homah.SetGrantOffset (grantOffset);
  homah.SetPayloadSize (0);
    
  if (genGRANT)
    homah.SetFlags (HomaHeader::Flags_t::GRANT);
  else if (genBUSY)
    homah.SetFlags (HomaHeader::Flags_t::BUSY);
    
  Ptr<Packet> p = Create<Packet> ();
  p-> AddHeader (homah);
  m_nanoPuArcht->GetArbiter ()->Receive(p, meta);
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

HomaNanoPuArchtIngressPipe::HomaNanoPuArchtIngressPipe (Ptr<NanoPuArchtReassemble> reassemble,
                                                      Ptr<NanoPuArchtPacketize> packetize,
                                                      Ptr<HomaNanoPuArchtPktGen> pktgen,
                                                      uint16_t rttPkts)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_reassemble = reassemble;
  m_packetize = packetize;
  m_pktgen = pktgen;
  m_rttPkts = rttPkts;
}

HomaNanoPuArchtIngressPipe::~HomaNanoPuArchtIngressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
bool HomaNanoPuArchtIngressPipe::IngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                                             uint16_t protocol, const Address &from)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU NDP IngressPipe received: " << 
                cp->ToString ());
  
  NS_ASSERT_MSG (protocol==0x0800,
                 "NdpNanoPuArcht works only with IPv4 packets!");
    
  Ipv4Header iph;
  cp->RemoveHeader (iph);  
  NdpHeader ndph;
  cp->RemoveHeader (ndph);
    
  uint16_t txMsgId = ndph.GetTxMsgId ();
  uint16_t pktOffset = ndph.GetPktOffset ();
  uint16_t msgLen = ndph.GetMsgLen ();
    
  if (ndph.GetFlags () & NdpHeader::Flags_t::DATA)
  {   
    bool genACK = false;
    bool genNACK = false;
    bool genPULL = false;
    Ipv4Address srcIp = iph.GetSource ();
    uint16_t srcPort = ndph.GetSrcPort ();
    uint16_t dstPort = ndph.GetDstPort ();
      
    rxMsgInfoMeta_t rxMsgInfo = m_reassemble->GetRxMsgInfo (srcIp, 
                                                            srcPort, 
                                                            txMsgId,
                                                            msgLen, 
                                                            pktOffset);
      
    // NOTE: The ackNo in the rxMsgInfo is the acknowledgement number
    //       before processing this incoming data packet because this
    //       packet has not updated the receivedBitmap in the reassembly
    //       buffer yet.
    uint16_t pullOffsetDiff;
    if (ndph.GetFlags () & NdpHeader::Flags_t::CHOP)
    {
      NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                   " NanoPU NDP IngressPipe processing chopped data packet.");
      genNACK = true;
      genPULL = true;
      pullOffsetDiff = 0;
    } 
    else 
    {
      NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                   " NanoPU NDP IngressPipe processing data packet.");
      genACK = true;
    
      if (pktOffset + m_rttPkts <= msgLen )
          genPULL = true;
        
      reassembleMeta_t metaData;
      metaData.rxMsgId = rxMsgInfo.rxMsgId;
      metaData.srcIp = srcIp;
      metaData.srcPort = srcPort;
      metaData.dstPort = dstPort;
      metaData.txMsgId = txMsgId;
      metaData.msgLen = msgLen;
      metaData.pktOffset = pktOffset;
        
      pullOffsetDiff = 1;
//       m_reassemble->ProcessNewPacket (cp, metaData);
      Simulator::Schedule (NanoSeconds(INGRESS_PIPE_DELAY), 
                           &NanoPuArchtReassemble::ProcessNewPacket, 
                           m_reassemble, cp, metaData);
    }
      
    // Compute pullOffset with a PRAW extern
    uint16_t pullOffset = 0;
    if (rxMsgInfo.isNewMsg)
    {
      m_credits[rxMsgInfo.rxMsgId] = m_rttPkts + pullOffsetDiff;
    }
    else
    {
      m_credits[rxMsgInfo.rxMsgId] += pullOffsetDiff;
    }
    pullOffset = m_credits[rxMsgInfo.rxMsgId];
      
    m_pktgen->CtrlPktEvent (genACK, genNACK, genPULL, 
                            srcIp, srcPort, dstPort, txMsgId,
                            msgLen, pktOffset, pullOffset);
  }  
  else // not a DATA packet
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU NDP IngressPipe processing a control packet.");
      
    if (ndph.GetFlags () & NdpHeader::Flags_t::ACK)
    {
      m_packetize->DeliveredEvent (txMsgId, msgLen, (1<<pktOffset));
    }
    if (ndph.GetFlags () & NdpHeader::Flags_t::PULL ||
        ndph.GetFlags () & NdpHeader::Flags_t::NACK)
    {
      int rtxPkt = (ndph.GetFlags () & NdpHeader::Flags_t::NACK) ? (int) pktOffset : -1;
      int credit = (ndph.GetFlags () & NdpHeader::Flags_t::PULL) ? (int) ndph.GetPullOffset () : -1;
      m_packetize->CreditToBtxEvent (txMsgId, rtxPkt, credit, credit,
                                     NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
                                     std::greater<int>());
    }
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

HomaNanoPuArchtEgressPipe::HomaNanoPuArchtEgressPipe (Ptr<NanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << nanoPuArcht);
    
  m_nanoPuArcht = nanoPuArcht;
}

HomaNanoPuArchtEgressPipe::~HomaNanoPuArchtEgressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HomaNanoPuArchtEgressPipe::EgressPipe (Ptr<const Packet> p, egressMeta_t meta)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
    
  SocketPriorityTag priorityTag;
  uint8_t priority;
  
  if (meta.isData)
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU Homa EgressPipe processing data packet.");
      
    HomaHeader homah;
    homah.SetSrcPort (meta.srcPort);
    homah.SetDstPort (meta.dstPort);
    homah.SetTxMsgId (meta.txMsgId);
    homah.SetMsgLen (meta.msgLen);
    homah.SetPktOffset (meta.pktOffset);
    // TODO: Use GrantOffset to signal for the allowed initial window size
    //       if this is a RPC request.
    homah.SetFlags (HomaHeader::Flags_t::DATA);
    homah.SetPayloadSize ((uint16_t) cp->GetSize ());
    // TODO: Set generation information on the packet. (Not essential)
    cp-> AddHeader (homah);
      
    // Priority of Data packets are determined by the packet tags
    // TODO: Determine the priority of response packets based on the 
    //       priority signalled by the control packets.
    priority = m_nanoPuArcht->GetPriority (meta.msgLen);
  }
  else
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU Homa EgressPipe processing control packet.");
    priority = 0; // Highest Priority
      
    HomaHeader homah;
    cp->RemoveHeader(homah);
    homah.SetPrio(m_nanoPuArcht->GetPriority (meta.msgLen));
    cp->AddHeader (homah);
  }
  
  Ptr<NetDevice> boundnetdevice = m_nanoPuArcht->GetBoundNetDevice ();
  Ptr<Node> node = m_nanoPuArcht->GetNode ();
  Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4> ();
  int32_t ifIndex = ipv4proto->GetInterfaceForDevice (boundnetdevice);
  Ipv4Address srcIP = ipv4proto->SourceAddressSelection (ifIndex, meta.dstIP);
    
  Ipv4Header iph;
  iph.SetSource (srcIP);
  iph.SetDestination (meta.dstIP);
  iph.SetPayloadSize (cp->GetSize ());
  iph.SetTos (priority);
  cp-> AddHeader (iph);
    
  priorityTag.SetPriority(priority);
  cp-> AddPacketTag (priorityTag);
  
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU Homa EgressPipe sending: " << 
                cp->ToString ());
    
//   return m_nanoPuArcht->SendToNetwork(cp, boundnetdevice->GetAddress ());
//   m_nanoPuArcht->SendToNetwork(cp);
  Simulator::Schedule (NanoSeconds(EGRESS_PIPE_DELAY), 
                       &NanoPuArcht::SendToNetwork, m_nanoPuArcht, cp);

  return;
}
    
/******************************************************************************/
       
TypeId HomaNanoPuArcht::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaNanoPuArcht")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HomaNanoPuArcht::HomaNanoPuArcht (Ptr<Node> node,
                                Ptr<NetDevice> device,
                                Time timeoutInterval,
                                uint16_t maxMessages,
                                uint16_t payloadSize,
                                uint16_t initialCredit,
                                uint16_t maxTimeoutCnt) : NanoPuArcht (node,
                                                                       device,
                                                                       timeoutInterval,
                                                                       maxMessages,
                                                                       payloadSize,
                                                                       initialCredit,
                                                                       maxTimeoutCnt)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  
  m_pktgen = CreateObject<HomaNanoPuArchtPktGen> (this);
  m_ingresspipe = CreateObject<HomaNanoPuArchtIngressPipe> (m_reassemble,
                                                            m_packetize,
                                                            m_pktgen,
                                                            initialCredit);
  m_egresspipe = CreateObject<HomaNanoPuArchtEgressPipe> (this);
    
  m_arbiter->SetEgressPipe(m_egresspipe);
}

HomaNanoPuArcht::~HomaNanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
bool HomaNanoPuArcht::EnterIngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                                       uint16_t protocol, const Address &from)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
    
  m_ingresspipe->IngressPipe (device, p, protocol, from);
    
  return true;
}
      
uint8_t HomaNanoPuArcht::GetPriority (uint16_t msgLen)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  
  uint8_t prio = 0;
  for (auto & threshold : HomaNanoPuArcht::m_priorities)
  {
    if (msgLen <= threshold)
      return prio;
      
    prio++;
  }
  return prio;
}
    
} // namespace ns3