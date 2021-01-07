/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2021 Stanford University
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

#include "hpcc-nanopu-transport.h"
#include "ns3/nanopu-archt.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-header.h"
#include "ns3/int-header.h"
#include "ns3/hpcc-header.h"

namespace ns3 {
    
NS_LOG_COMPONENT_DEFINE ("HpccNanoPuArcht");

NS_OBJECT_ENSURE_REGISTERED (HpccNanoPuArcht);

/******************************************************************************/

TypeId HpccNanoPuArchtPktGen::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HpccNanoPuArchtPktGen")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HpccNanoPuArchtPktGen::HpccNanoPuArchtPktGen (Ptr<NanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
}

HpccNanoPuArchtPktGen::~HpccNanoPuArchtPktGen ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HpccNanoPuArchtPktGen::CtrlPktEvent (bool genACK, bool genNACK, bool genPULL,
                                          Ipv4Address dstIp, uint16_t dstPort, 
                                          uint16_t srcPort, uint16_t txMsgId, 
                                          uint16_t msgLen, uint16_t pktOffset, 
                                          uint16_t pullOffset)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
//   NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
//                " NanoPU NDP PktGen processing CtrlPktEvent." <<
//                " GenACK: " << genACK << " GenNACK: " << genNACK <<
//                " GenPULL: " << genPULL);
    
//   Time delay = Time(0);
    
//   egressMeta_t meta;
//   meta.isData = false;
//   meta.dstIP = dstIp;
    
//   NdpHeader ndph;
//   ndph.SetSrcPort (srcPort);
//   ndph.SetDstPort (dstPort);
//   ndph.SetTxMsgId (txMsgId);
//   ndph.SetMsgLen (msgLen);
//   ndph.SetPktOffset (pktOffset);
//   ndph.SetPullOffset (pullOffset);
//   ndph.SetPayloadSize (0);
    
//   if (genPULL)
//   {
//     Time now = Simulator::Now ();
//     Time txTime = m_pacerLastTxTime + m_packetTxTime;
    
//     if (now < txTime)
//     {
//       delay = txTime - now;
//       m_pacerLastTxTime = txTime;
//     }
//     else
//     {
//       m_pacerLastTxTime = now;
//     }
      
//     ndph.SetFlags (NdpHeader::Flags_t::PULL);
    
//     if (genACK && delay==Time(0))
//     {
//       ndph.SetFlags (ndph.GetFlags () | NdpHeader::Flags_t::ACK);
//       genACK = false;
//     }
//     if (genNACK && delay==Time(0))
//     {
//       ndph.SetFlags (ndph.GetFlags () | NdpHeader::Flags_t::NACK);
//       genNACK = false;
//     }
    
//     Ptr<Packet> p = Create<Packet> ();
//     p-> AddHeader (ndph);
    
//     Ptr<NanoPuArchtArbiter> arbiter = m_nanoPuArcht->GetArbiter ();
//     Simulator::Schedule (delay, &NanoPuArchtArbiter::Receive, arbiter, p, meta);
    
// //     NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
// //                   " NanoPU NDP PktGen generated: " << 
// //                   p->ToString ());
     
//   }
    
//   if (genACK)
//   {   
//     ndph.SetFlags (NdpHeader::Flags_t::ACK);
      
//     Ptr<Packet> p = Create<Packet> ();
//     p-> AddHeader (ndph);
//     m_nanoPuArcht->GetArbiter ()->Receive(p, meta);
//   }
//   if (genNACK)
//   {
//     ndph.SetFlags (NdpHeader::Flags_t::NACK);
      
//     Ptr<Packet> p = Create<Packet> ();
//     p-> AddHeader (ndph);
//     m_nanoPuArcht->GetArbiter ()->Receive(p, meta);
//   }
}

/******************************************************************************/
 
TypeId HpccNanoPuArchtIngressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HpccNanoPuArchtIngressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HpccNanoPuArchtIngressPipe::HpccNanoPuArchtIngressPipe (Ptr<NanoPuArchtReassemble> reassemble,
                                                        Ptr<NanoPuArchtPacketize> packetize,
                                                        Ptr<HpccNanoPuArchtPktGen> pktgen,
                                                        uint16_t rttPkts)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_reassemble = reassemble;
  m_packetize = packetize;
  m_pktgen = pktgen;
  m_rttPkts = rttPkts;
}

HpccNanoPuArchtIngressPipe::~HpccNanoPuArchtIngressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
bool HpccNanoPuArchtIngressPipe::IngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                                              uint16_t protocol, const Address &from)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU HPCC IngressPipe received: " << 
                cp->ToString ());
  
  NS_ASSERT_MSG (protocol==0x0800,
                 "HpccNanoPuArcht works only with IPv4 packets!");
    
//   Ipv4Header iph;
//   cp->RemoveHeader (iph);  
    
//   NS_ASSERT_MSG(iph.GetProtocol() == NdpHeader::PROT_NUMBER,
//                   "This ingress pipeline only works for NDP Transport");
    
//   NdpHeader ndph;
//   cp->RemoveHeader (ndph);
    
//   uint16_t txMsgId = ndph.GetTxMsgId ();
//   uint16_t pktOffset = ndph.GetPktOffset ();
//   uint16_t msgLen = ndph.GetMsgLen ();
    
//   if (ndph.GetFlags () & NdpHeader::Flags_t::DATA)
//   {   
//     bool genACK = false;
//     bool genNACK = false;
//     bool genPULL = false;
//     Ipv4Address srcIp = iph.GetSource ();
//     uint16_t srcPort = ndph.GetSrcPort ();
//     uint16_t dstPort = ndph.GetDstPort ();
      
//     rxMsgInfoMeta_t rxMsgInfo = m_reassemble->GetRxMsgInfo (srcIp, 
//                                                             srcPort, 
//                                                             txMsgId,
//                                                             msgLen, 
//                                                             pktOffset);
      
//     // NOTE: The ackNo in the rxMsgInfo is the acknowledgement number
//     //       before processing this incoming data packet because this
//     //       packet has not updated the receivedBitmap in the reassembly
//     //       buffer yet.
//     uint16_t pullOffsetDiff;
//     if (ndph.GetFlags () & NdpHeader::Flags_t::CHOP)
//     {
//       NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
//                    " NanoPU NDP IngressPipe processing chopped data packet.");
//       genNACK = true;
//       genPULL = true;
//       pullOffsetDiff = 0;
//     } 
//     else 
//     {
//       NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
//                    " NanoPU NDP IngressPipe processing data packet.");
//       genACK = true;
    
//       if (pktOffset + m_rttPkts <= msgLen )
//           genPULL = true;
        
//       reassembleMeta_t metaData;
//       metaData.rxMsgId = rxMsgInfo.rxMsgId;
//       metaData.srcIp = srcIp;
//       metaData.srcPort = srcPort;
//       metaData.dstPort = dstPort;
//       metaData.txMsgId = txMsgId;
//       metaData.msgLen = msgLen;
//       metaData.pktOffset = pktOffset;
        
//       pullOffsetDiff = 1;
// //       m_reassemble->ProcessNewPacket (cp, metaData);
//       Simulator::Schedule (NanoSeconds(INGRESS_PIPE_DELAY), 
//                            &NanoPuArchtReassemble::ProcessNewPacket, 
//                            m_reassemble, cp, metaData);
//     }
      
//     // Compute pullOffset with a PRAW extern
//     uint16_t pullOffset = 0;
//     if (rxMsgInfo.isNewMsg)
//     {
//       m_credits[rxMsgInfo.rxMsgId] = m_rttPkts + pullOffsetDiff;
//     }
//     else
//     {
//       m_credits[rxMsgInfo.rxMsgId] += pullOffsetDiff;
//     }
//     pullOffset = m_credits[rxMsgInfo.rxMsgId];
      
//     m_pktgen->CtrlPktEvent (genACK, genNACK, genPULL, 
//                             srcIp, srcPort, dstPort, txMsgId,
//                             msgLen, pktOffset, pullOffset);
// //     Simulator::Schedule (NanoSeconds(INGRESS_PIPE_DELAY), 
// //                          &NdpNanoPuArchtPktGen::CtrlPktEvent, 
// //                          m_pktgen, genACK, genNACK, genPULL, 
// //                          srcIp, srcPort, dstPort, txMsgId,
// //                          msgLen, pktOffset, pullOffset);  
    
//   }  
//   else // not a DATA packet
//   {
//     NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
//                  " NanoPU NDP IngressPipe processing a control packet.");
      
//     if (ndph.GetFlags () & NdpHeader::Flags_t::ACK)
//     {
//       m_packetize->DeliveredEvent (txMsgId, msgLen, (((bitmap_t)1)<<pktOffset));
// //       Simulator::Schedule (NanoSeconds(INGRESS_PIPE_DELAY), 
// //                            &NanoPuArchtPacketize::DeliveredEvent, 
// //                            m_packetize, txMsgId, msgLen, (1<<pktOffset));
//     }
//     if (ndph.GetFlags () & NdpHeader::Flags_t::PULL ||
//         ndph.GetFlags () & NdpHeader::Flags_t::NACK)
//     {
//       int rtxPkt = (ndph.GetFlags () & NdpHeader::Flags_t::NACK) ? (int) pktOffset : -1;
//       int credit = (ndph.GetFlags () & NdpHeader::Flags_t::PULL) ? (int) ndph.GetPullOffset () : -1;
//       m_packetize->CreditToBtxEvent (txMsgId, rtxPkt, credit, credit,
//                                      NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
//                                      std::greater<int>());
// //       Simulator::Schedule (NanoSeconds(INGRESS_PIPE_DELAY), 
// //                            &NanoPuArchtPacketize::CreditToBtxEvent, 
// //                            m_packetize, txMsgId, rtxPkt, credit, credit,
// //                            NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
// //                            std::greater<int>());
//     }
//   }
    
//   return true;
  return false;
}
    
/******************************************************************************/
    
TypeId HpccNanoPuArchtEgressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HpccNanoPuArchtEgressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HpccNanoPuArchtEgressPipe::HpccNanoPuArchtEgressPipe (Ptr<NanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << nanoPuArcht);
    
  m_nanoPuArcht = nanoPuArcht;
}

HpccNanoPuArchtEgressPipe::~HpccNanoPuArchtEgressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HpccNanoPuArchtEgressPipe::EgressPipe (Ptr<const Packet> p, egressMeta_t meta)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
  
  if (meta.isData)
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC EgressPipe processing data packet.");
        
    HpccHeader hpcch;
    hpcch.SetSrcPort (meta.srcPort);
    hpcch.SetDstPort (meta.dstPort);
    hpcch.SetTxMsgId (meta.txMsgId);
    hpcch.SetFlags (HpccHeader::Flags_t::DATA);
    hpcch.SetPktOffset (meta.pktOffset);
    hpcch.SetMsgSize ((uint32_t)meta.msgLen); // MsgSize in pkts is given by meta
    hpcch.SetPayloadSize ((uint16_t) cp->GetSize ());
    cp-> AddHeader (hpcch);
      
    IntHeader inth;
    inth.SetProtocol(HpccHeader::PROT_NUMBER);
    inth.SetPayloadSize ((uint16_t) cp->GetSize ());
    cp->AddHeader (inth);
  }
  else
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC EgressPipe processing control packet.");
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
  iph.SetTtl (64);
  iph.SetProtocol (IntHeader::PROT_NUMBER);
  cp-> AddHeader (iph);
  
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
                " NanoPU HPCC EgressPipe sending: " << 
                cp->ToString ());
    
//   return m_nanoPuArcht->SendToNetwork(cp, boundnetdevice->GetAddress ());
//   m_nanoPuArcht->SendToNetwork(cp);
  Simulator::Schedule (NanoSeconds(HPCC_EGRESS_PIPE_DELAY), 
                       &NanoPuArcht::SendToNetwork, m_nanoPuArcht, cp);

  return;
}
    
/******************************************************************************/
       
TypeId HpccNanoPuArcht::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HpccNanoPuArcht")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HpccNanoPuArcht::HpccNanoPuArcht (Ptr<Node> node,
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
  
  m_pktgen = CreateObject<HpccNanoPuArchtPktGen> (this);
  m_ingresspipe = CreateObject<HpccNanoPuArchtIngressPipe> (m_reassemble,
                                                            m_packetize,
                                                            m_pktgen,
                                                            initialCredit);
  m_egresspipe = CreateObject<HpccNanoPuArchtEgressPipe> (this);
    
  m_arbiter->SetEgressPipe (m_egresspipe);
}

HpccNanoPuArcht::~HpccNanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
bool HpccNanoPuArcht::EnterIngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                                        uint16_t protocol, const Address &from)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
    
  m_ingresspipe->IngressPipe (device, p, protocol, from);
    
  return true;
}
    
} // namespace ns3