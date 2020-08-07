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
#include "ndp-nanopu-transport.h"
#include "ns3/ipv4-header.h"
#include "ns3/ndp-header.h"

namespace ns3 {
    
NS_LOG_COMPONENT_DEFINE ("NdpNanoPuArcht");

NS_OBJECT_ENSURE_REGISTERED (NdpNanoPuArcht);

/******************************************************************************/

TypeId NdpNanoPuArchtPktGen::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NdpNanoPuArchtPktGen")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NdpNanoPuArchtPktGen::NdpNanoPuArchtPktGen (Ptr<NanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
    
  Ptr<NetDevice> netDevice = m_nanoPuArcht->GetBoundNetDevice ();
  PointToPointNetDevice* p2pNetDevice = dynamic_cast<PointToPointNetDevice*>(&(*(netDevice))); 
  
  DataRate dataRate = p2pNetDevice->GetDataRate ();
  uint16_t mtuBytes = m_nanoPuArcht->GetBoundNetDevice ()->GetMtu ();
  m_packetTxTime = dataRate.CalculateBytesTxTime ((uint32_t) mtuBytes);
    
  // Set an initial value for the last Tx time.
  m_pacerLastTxTime = Simulator::Now () - m_packetTxTime;
}

NdpNanoPuArchtPktGen::~NdpNanoPuArchtPktGen ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this);
}
    
void NdpNanoPuArchtPktGen::CtrlPktEvent (bool genACK, bool genNACK, bool genPULL,
                                         Ipv4Address dstIp, uint16_t dstPort, 
                                         uint16_t srcPort, uint16_t txMsgId, 
                                         uint16_t msgLen, uint16_t pktOffset, 
                                         uint16_t pullOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this);
  NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << 
               " NanoPU NDP PktGen processing CtrlPktEvent. " <<
               "GenACK: " << genACK << " GenNACK: " << genNACK <<
               " GenPULL: " << genPULL);
    
  Time delay = Time(0);
    
  Ptr<Packet> p = Create<Packet> ();
    
  egressMeta_t meta;
  meta.isData = false;
  meta.dstIP = dstIp;
    
  NdpHeader ndph;
  ndph.SetSrcPort (srcPort);
  ndph.SetDstPort (dstPort);
  ndph.SetTxMsgId (txMsgId);
  ndph.SetMsgLen (msgLen);
  ndph.SetPktOffset (pktOffset);
  ndph.SetPullOffset (pullOffset);
  ndph.SetPayloadSize (0);
    
  if (genPULL)
  {
    Time now = Simulator::Now ();
    Time txTime = m_pacerLastTxTime + m_packetTxTime;
    
    if (now < txTime)
    {
      delay = txTime - now;
      m_pacerLastTxTime = txTime;
    }
    else
    {
      m_pacerLastTxTime = now;
    }
      
    ndph.SetFlags (NdpHeader::Flags_t::PULL);
    
    if (genACK && delay==Time(0))
    {
      ndph.SetFlags (ndph.GetFlags () | NdpHeader::Flags_t::ACK);
      genACK = false;
    }
    if (genNACK && delay==Time(0))
    {
      ndph.SetFlags (ndph.GetFlags () | NdpHeader::Flags_t::NACK);
      genNACK = false;
    }
      
    p-> AddHeader (ndph);
    
    Ptr<NanoPuArchtArbiter> arbiter = m_nanoPuArcht->GetArbiter ();
    Simulator::Schedule (delay, &NanoPuArchtArbiter::Receive, arbiter, p, meta);
    
//     NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << 
//                   " NanoPU NDP PktGen generated: " << 
//                   p->ToString ());
  }
    
  if (genACK)
  {
    ndph.SetFlags (NdpHeader::Flags_t::ACK);
    p-> AddHeader (ndph);
    m_nanoPuArcht->GetArbiter ()->Receive(p, meta);
  }
  if (genNACK)
  {
    ndph.SetFlags (NdpHeader::Flags_t::NACK);
    p-> AddHeader (ndph);
    m_nanoPuArcht->GetArbiter ()->Receive(p, meta);
  }
}

/******************************************************************************/
 
TypeId NdpNanoPuArchtIngressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NdpNanoPuArchtIngressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NdpNanoPuArchtIngressPipe::NdpNanoPuArchtIngressPipe (Ptr<NanoPuArchtReassemble> reassemble,
                                                      Ptr<NanoPuArchtPacketize> packetize,
                                                      Ptr<NdpNanoPuArchtPktGen> pktgen,
                                                      uint16_t rttPkts)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this);
    
  m_reassemble = reassemble;
  m_packetize = packetize;
  m_pktgen = pktgen;
  m_rttPkts = rttPkts;
}

NdpNanoPuArchtIngressPipe::~NdpNanoPuArchtIngressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this);
}
    
bool NdpNanoPuArchtIngressPipe::IngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                                             uint16_t protocol, const Address &from)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this << cp);
  NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << 
               " NanoPU NDP IngressPipe received: " << 
                cp->ToString ());
    
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
      NS_LOG_LOGIC(Simulator::Now ().GetSeconds () << 
                   " NanoPU NDP IngressPipe processing chopped data packet.");
      genNACK = true;
      genPULL = true;
      pullOffsetDiff = 0;
    } 
    else 
    {
      NS_LOG_LOGIC(Simulator::Now ().GetSeconds () << 
                   " NanoPU NDP IngressPipe processing data packet.");
      genACK = true;
      // TODO: No need to generate new PULL packets if this was the last
      //       packet of the message (ie. if ackNo > msgLen)
      genPULL = true;
        
      reassembleMeta_t metaData;
      metaData.rxMsgId = rxMsgInfo.rxMsgId;
      metaData.srcIp = srcIp;
      metaData.srcPort = srcPort;
      metaData.txMsgId = txMsgId;
      metaData.msgLen = msgLen;
      metaData.pktOffset = pktOffset;
        
      pullOffsetDiff = 1;
      m_reassemble->ProcessNewPacket (cp, metaData);
    }
      
    // Compute pullOffset with a PRAW extern
    uint16_t pullOffset = 0;
    if (rxMsgInfo.isNewMsg)
    {
      m_credits.emplace(rxMsgInfo.rxMsgId, m_rttPkts + pullOffsetDiff);
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
    NS_LOG_LOGIC(Simulator::Now ().GetSeconds () << 
                 " NanoPU NDP IngressPipe processing a control packet.");
      
    if (ndph.GetFlags () & NdpHeader::Flags_t::ACK)
    {
      bool isInterval = false;
      m_packetize->DeliveredEvent (txMsgId, pktOffset, isInterval, msgLen);
    }
    else if (ndph.GetFlags () & NdpHeader::Flags_t::PULL ||
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
    
TypeId NdpNanoPuArchtEgressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NdpNanoPuArchtEgressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NdpNanoPuArchtEgressPipe::NdpNanoPuArchtEgressPipe (Ptr<NanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
}

NdpNanoPuArchtEgressPipe::~NdpNanoPuArchtEgressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this);
}
    
bool NdpNanoPuArchtEgressPipe::EgressPipe (Ptr<const Packet> p, egressMeta_t meta)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this << cp);
  
  if (meta.isData)
  {
    NS_LOG_LOGIC(Simulator::Now ().GetSeconds () << 
                 " NanoPU NDP EgressPipe processing data packet.");
      
    NdpHeader ndph;
    ndph.SetSrcPort (meta.srcPort);
    ndph.SetDstPort (meta.dstPort);
    ndph.SetTxMsgId (meta.txMsgId);
    ndph.SetMsgLen (meta.msgLen);
    ndph.SetPktOffset (meta.pktOffset);
    ndph.SetFlags (NdpHeader::Flags_t::DATA);
    ndph.SetPayloadSize ((uint16_t) cp->GetSize ());
    cp-> AddHeader (ndph);
  }
  else
  {
    NS_LOG_LOGIC(Simulator::Now ().GetSeconds () << 
                 " NanoPU NDP EgressPipe processing control packet.");
  }
  
  Ptr<NetDevice> boundnetdevice = m_nanoPuArcht->GetBoundNetDevice ();
    
  Ipv4Header iph;
  Ptr<Node> node = m_nanoPuArcht->GetNode ();
  Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4> ();
  int32_t ifIndex = ipv4proto->GetInterfaceForDevice (boundnetdevice);
  Ipv4Address srcIP = ipv4proto->SourceAddressSelection (ifIndex, meta.dstIP);
  iph.SetSource (srcIP);
  iph.SetDestination (meta.dstIP);
  cp-> AddHeader (iph);
  
  NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << 
               " NanoPU NDP EgressPipe sending: " << 
                cp->ToString ());
    
  return m_nanoPuArcht->Send(cp, boundnetdevice->GetAddress ());
}
    
/******************************************************************************/
       
TypeId NdpNanoPuArcht::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NdpNanoPuArcht")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NdpNanoPuArcht::NdpNanoPuArcht (Ptr<Node> node,
                                Ptr<NetDevice> device,
                                uint16_t maxMessages,
                                uint16_t initialCredit) : NanoPuArcht (node,
                                                                       device,
                                                                       maxMessages,
                                                                       initialCredit)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this);
  
  m_pktgen = CreateObject<NdpNanoPuArchtPktGen> (this);
  m_ingresspipe = CreateObject<NdpNanoPuArchtIngressPipe> (m_reassemble,
                                                           m_packetize,
                                                           m_pktgen,
                                                           initialCredit);
  m_egresspipe = CreateObject<NdpNanoPuArchtEgressPipe> (this);
    
  m_arbiter->SetEgressPipe(m_egresspipe);
}

NdpNanoPuArcht::~NdpNanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this);
}
    
bool NdpNanoPuArcht::EnterIngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                                    uint16_t protocol, const Address &from)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetSeconds () << this << p);
    
  m_ingresspipe->IngressPipe (device, p, protocol, from);
    
  return true;
}
    
} // namespace ns3