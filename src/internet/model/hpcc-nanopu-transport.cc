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
#include "ns3/uinteger.h"
#include "ns3/double.h"

#include "hpcc-nanopu-transport.h"
#include "ns3/nanopu-archt.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-header.h"
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
    
void HpccNanoPuArchtPktGen::CtrlPktEvent (Ipv4Address dstIp, uint16_t dstPort, uint16_t srcPort,
                                          uint16_t txMsgId, uint16_t pktOffset, uint16_t msgLen,
                                          IntHeader receivedIntHeader)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU HPCC PktGen processing CtrlPktEvent." <<
               " txMsgId: " << txMsgId << " pktOffset: " << pktOffset);
    
  egressMeta_t meta;
  meta.isData = false;
  meta.dstIP = dstIp;
    
  Ptr<Packet> p = Create<Packet> ();
  receivedIntHeader.SetProtocol(0); // Nothing else will exist after this in an ACK packet.
  p-> AddHeader (receivedIntHeader); // This will be payload of HPCC header.
    
  HpccHeader hpcch;
  hpcch.SetSrcPort (srcPort);
  hpcch.SetDstPort (dstPort);
  hpcch.SetTxMsgId (txMsgId);
  hpcch.SetFlags (HpccHeader::Flags_t::ACK);
  hpcch.SetPktOffset (pktOffset);
  hpcch.SetMsgSize (msgLen);
  hpcch.SetPayloadSize ((uint16_t)p->GetSize ());
  p-> AddHeader (hpcch); 
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
                " NanoPU HPCC PktGen generated: " << p->ToString ());
    
  m_nanoPuArcht->GetArbiter ()->Receive(p, meta);
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
                                                        Time baseRtt, uint32_t mtu,
                                                        uint32_t initWin, uint32_t winAI,
                                                        double utilFac, uint16_t maxStage)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_reassemble = reassemble;
  m_packetize = packetize;
  m_pktgen = pktgen;
    
  m_baseRTT = baseRtt;
  m_mtu = mtu;
  m_initWin = initWin;
  m_winAI = winAI;
  m_utilFac = utilFac;
  m_maxStage = maxStage;
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
    
  Ipv4Header iph;
  cp->RemoveHeader (iph);  
    
  NS_ASSERT_MSG(iph.GetProtocol() == IntHeader::PROT_NUMBER,
                "This ingress pipeline only works for HPCC Transport "
                "which requires an INT header to be appended after the IPv4 header!");
    
  IntHeader inth;
  cp->RemoveHeader (inth);
    
  NS_ASSERT_MSG(inth.GetProtocol() == HpccHeader::PROT_NUMBER,
                "This ingress pipeline only works for HPCC Transport "
                "which requires an HPCC header to be appended after the INT header!");
    
  HpccHeader hpcch;
  cp->RemoveHeader (hpcch);
    
  uint16_t txMsgId = hpcch.GetTxMsgId ();
  uint16_t pktOffset = hpcch.GetPktOffset ();
  uint16_t msgLen = (uint16_t)hpcch.GetMsgSize (); // Msg Len is in pkts for nanoPU Archt
    
  if (hpcch.GetFlags () & HpccHeader::Flags_t::DATA)
  {   
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC IngressPipe processing a DATA packet.");
      
    Ipv4Address srcIp = iph.GetSource ();
    uint16_t srcPort = hpcch.GetSrcPort ();
    uint16_t dstPort = hpcch.GetDstPort ();
      
    rxMsgInfoMeta_t rxMsgInfo = m_reassemble->GetRxMsgInfo (srcIp, 
                                                            srcPort, 
                                                            txMsgId,
                                                            msgLen, 
                                                            pktOffset);
    
    uint16_t ackNo = rxMsgInfo.ackNo;
    if (rxMsgInfo.ackNo == pktOffset)
      ackNo++;
        
    reassembleMeta_t metaData;
    metaData.rxMsgId = rxMsgInfo.rxMsgId;
    metaData.srcIp = srcIp;
    metaData.srcPort = srcPort;
    metaData.dstPort = dstPort;
    metaData.txMsgId = txMsgId;
    metaData.msgLen = msgLen;
    metaData.pktOffset = pktOffset;

//     m_reassemble->ProcessNewPacket (cp, metaData);
    Simulator::Schedule (NanoSeconds(HPCC_INGRESS_PIPE_DELAY), 
                         &NanoPuArchtReassemble::ProcessNewPacket, 
                         m_reassemble, cp, metaData);
      
    m_pktgen->CtrlPktEvent (srcIp, srcPort, dstPort, txMsgId, ackNo, msgLen, inth);  
    
  }  
  else if (hpcch.GetFlags () & HpccHeader::Flags_t::ACK)
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC IngressPipe processing an ACK packet.");
    
    if (m_ackNos.find(txMsgId) == m_ackNos.end())
    {
      // This is a new message
      m_ackNos[txMsgId] = pktOffset;
      m_winSizes[txMsgId] = m_initWin;
      m_lastUpdateSeqs[txMsgId] = 0;
      m_incStages[txMsgId] = 0;
    }
    else
    {
      // This is not a new message
      if (pktOffset > m_ackNos[txMsgId])
        m_ackNos[txMsgId] = pktOffset;
    }
    
      
    m_packetize->DeliveredEvent (txMsgId, msgLen, setBitMapUntil(m_ackNos[txMsgId]));
//     Simulator::Schedule (NanoSeconds(INGRESS_PIPE_DELAY), 
//                          &NanoPuArchtPacketize::DeliveredEvent, 
//                          m_packetize, txMsgId, msgLen, setBitMapUntil(pktOffset));
      
    // TODO: Calculate the new credit (ackNo + winSize) and trigger CreditToBtxEvent
    //       as shown below.
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
      
    if (pktOffset >= msgLen)
    {
      // The message has been fully acknowledged.
      m_ackNos.erase(txMsgId);
      m_winSizes.erase(txMsgId);
      m_lastUpdateSeqs.erase(txMsgId);
      m_incStages.erase(txMsgId);
      m_prevIntHdrs.erase(txMsgId);
        
      // TODO: Erasing state values would be an expensive solution.
      //       Instead, a mechanism to determine whether the current
      //       state is valid or it belogs to an old (completed or 
      //       expired) message.
    }
    else
    {
      m_prevIntHdrs[txMsgId] = inth;
    }
  }
  else
  {
    NS_LOG_WARN(Simulator::Now ().GetNanoSeconds () << 
                " NanoPU HPCC IngressPipe received an unknown type of packet!");
    return false;
  }
    
  return true;
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
      
  }
  else
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC EgressPipe processing control packet.");
  }
    
  IntHeader inth;
  inth.SetProtocol(HpccHeader::PROT_NUMBER);
  inth.SetPayloadSize ((uint16_t) cp->GetSize ());
  cp->AddHeader (inth);
  
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
                                  uint16_t maxTimeoutCnt,
                                  Time baseRtt,
                                  uint32_t winAI,
                                  double utilFac,
                                  uint16_t maxStage) : NanoPuArcht (node, device,
                                                                    timeoutInterval,
                                                                    maxMessages,
                                                                    payloadSize,
                                                                    initialCredit,
                                                                    maxTimeoutCnt)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  
  m_pktgen = CreateObject<HpccNanoPuArchtPktGen> (this);
    
  uint32_t initWin = (uint32_t)initialCredit * (uint32_t)payloadSize;
  m_ingresspipe = CreateObject<HpccNanoPuArchtIngressPipe> (m_reassemble, m_packetize,
                                                            m_pktgen, baseRtt, 
                                                            device->GetMtu (),
                                                            initWin, winAI,
                                                            utilFac, maxStage);
    
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