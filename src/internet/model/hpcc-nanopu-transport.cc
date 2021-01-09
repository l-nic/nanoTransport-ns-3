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
                                                        double baseRtt, uint32_t mtu,
                                                        uint16_t initCredit, uint32_t winAI,
                                                        double utilFac, uint16_t maxStage)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_reassemble = reassemble;
  m_packetize = packetize;
  m_pktgen = pktgen;
    
  m_baseRTT = baseRtt;
  m_mtu = mtu;
  m_initCredit = initCredit;
  m_winAI = winAI;
  m_utilFac = utilFac;
  m_maxStage = maxStage;
}

HpccNanoPuArchtIngressPipe::~HpccNanoPuArchtIngressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
uint16_t HpccNanoPuArchtIngressPipe::ComputeNumPkts (uint32_t winSizeBytes)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << winSizeBytes);
    
  // TODO: The division operation below can be handled with
  //       a lookup table in a programmable hardware pipeline.
  return winSizeBytes / m_mtu + (winSizeBytes % m_mtu != 0);
}
    
double HpccNanoPuArchtIngressPipe::MeasureInflight (uint16_t txMsgId, 
                                                    IntHeader intHdr)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  // TODO: A P4 programmable pipeline wouldn't have double type variables.
  //       Maybe we can represent utilization as an integer from 0 to 100?
    
  // TODO: The code below uses cascaded if statements to prevent the use
  //       of for loop. This design assumes a particular MAX_INT_HOPS value.
    
  // TODO: The code below uses relatively complex computations. We could 
  //       probably use lookup tables for such computations?
    
  uint16_t nHops = intHdr.GetNHops ();
  intHop_t curHopInfo;
  intHop_t oldHopInfo;
  double maxUtil = 0.0;
  double curUtil;
  double tao;
  double curTao;
  double txRate;
    
  uint16_t curHopIdx = 0;
  if (curHopIdx < nHops)
  {
    curHopInfo = intHdr.PeekHopN (curHopIdx);
    oldHopInfo = m_prevIntHdrs[txMsgId].PeekHopN (curHopIdx);
      
    curTao = (double)(curHopInfo.time - oldHopInfo.time) *1e-9; //Converted to seconds
    txRate = (double)(curHopInfo.txBytes - oldHopInfo.txBytes) / curTao;
      
    curUtil = (double)std::min(curHopInfo.qlen, oldHopInfo.qlen)
              / ((double)curHopInfo.bitRate * m_baseRTT);
    curUtil += txRate / (double)curHopInfo.bitRate;
      
    if (curUtil > maxUtil)
    {
      maxUtil = curUtil;
      tao = curTao;
    }
  }
    
  curHopIdx++;
  if (curHopIdx < nHops)
  {
    curHopInfo = intHdr.PeekHopN (curHopIdx);
    oldHopInfo = m_prevIntHdrs[txMsgId].PeekHopN (curHopIdx);
      
    curTao = (double)(curHopInfo.time - oldHopInfo.time) *1e-9; //Converted to seconds
    txRate = (double)(curHopInfo.txBytes - oldHopInfo.txBytes) / curTao;
      
    curUtil = (double)std::min(curHopInfo.qlen, oldHopInfo.qlen)
              / ((double)curHopInfo.bitRate * m_baseRTT);
    curUtil += txRate / (double)curHopInfo.bitRate;
      
    if (curUtil > maxUtil)
    {
      maxUtil = curUtil;
      tao = curTao;
    }
  }
    
  curHopIdx++;
  if (curHopIdx < nHops)
  {
    curHopInfo = intHdr.PeekHopN (curHopIdx);
    oldHopInfo = m_prevIntHdrs[txMsgId].PeekHopN (curHopIdx);
      
    curTao = (double)(curHopInfo.time - oldHopInfo.time) *1e-9; //Converted to seconds
    txRate = (double)(curHopInfo.txBytes - oldHopInfo.txBytes) / curTao;
      
    curUtil = (double)std::min(curHopInfo.qlen, oldHopInfo.qlen)
              / ((double)curHopInfo.bitRate * m_baseRTT);
    curUtil += txRate / (double)curHopInfo.bitRate;
      
    if (curUtil > maxUtil)
    {
      maxUtil = curUtil;
      tao = curTao;
    }
  }
    
  curHopIdx++;
  if (curHopIdx < nHops)
  {
    curHopInfo = intHdr.PeekHopN (curHopIdx);
    oldHopInfo = m_prevIntHdrs[txMsgId].PeekHopN (curHopIdx);
      
    curTao = (double)(curHopInfo.time - oldHopInfo.time) *1e-9; //Converted to seconds
    txRate = (double)(curHopInfo.txBytes - oldHopInfo.txBytes) / curTao;
      
    curUtil = (double)std::min(curHopInfo.qlen, oldHopInfo.qlen)
              / ((double)curHopInfo.bitRate * m_baseRTT);
    curUtil += txRate / (double)curHopInfo.bitRate;
      
    if (curUtil > maxUtil)
    {
      maxUtil = curUtil;
      tao = curTao;
    }
  }
    
  curHopIdx++;
  if (curHopIdx < nHops)
  {
    curHopInfo = intHdr.PeekHopN (curHopIdx);
    oldHopInfo = m_prevIntHdrs[txMsgId].PeekHopN (curHopIdx);
      
    curTao = (double)(curHopInfo.time - oldHopInfo.time) *1e-9; //Converted to seconds
    txRate = (double)(curHopInfo.txBytes - oldHopInfo.txBytes) / curTao;
      
    curUtil = (double)std::min(curHopInfo.qlen, oldHopInfo.qlen)
              / ((double)curHopInfo.bitRate * m_baseRTT);
    curUtil += txRate / (double)curHopInfo.bitRate;
      
    if (curUtil > maxUtil)
    {
      maxUtil = curUtil;
      tao = curTao;
    }
  }
    
  curHopIdx++;
  if (curHopIdx < nHops)
  {
    curHopInfo = intHdr.PeekHopN (curHopIdx);
    oldHopInfo = m_prevIntHdrs[txMsgId].PeekHopN (curHopIdx);
      
    curTao = (double)(curHopInfo.time - oldHopInfo.time) *1e-9; //Converted to seconds
    txRate = (double)(curHopInfo.txBytes - oldHopInfo.txBytes) / curTao;
      
    curUtil = (double)std::min(curHopInfo.qlen, oldHopInfo.qlen)
              / ((double)curHopInfo.bitRate * m_baseRTT);
    curUtil += txRate / (double)curHopInfo.bitRate;
      
    if (curUtil > maxUtil)
    {
      maxUtil = curUtil;
      tao = curTao;
    }
  }
    
  curHopIdx++;
  NS_ASSERT_MSG(curHopIdx >= IntHeader::MAX_INT_HOPS,
                "NanoPU HPCC Ingress pipeline is not programmed to process "
                "all INT hop information!");
    
  tao = std::min(tao, m_baseRTT);
  m_utilizations[txMsgId] = (1.0 - tao/m_baseRTT) * m_utilizations[txMsgId]
                            + tao/m_baseRTT * maxUtil;
    
  return m_utilizations[txMsgId];
}
    
uint32_t HpccNanoPuArchtIngressPipe::ComputeWind (uint16_t txMsgId,
                                                  double utilization, 
                                                  bool updateWc)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () 
                   << this << utilization << updateWc);
    
  uint32_t winSize;
  if (utilization > m_utilFac || m_incStages[txMsgId] >= m_maxStage)
  {
    winSize = m_winSizes[txMsgId] / (utilization / m_utilFac) + m_winAI;
    if (updateWc)
    {
      m_incStages[txMsgId] = 0;
      m_winSizes[txMsgId] = winSize;
    }
  }
  else
  {
    winSize = m_winSizes[txMsgId] + m_winAI;
    if (updateWc)
    {
      m_incStages[txMsgId]++;
      m_winSizes[txMsgId] = winSize;
    }
  }
    
  return winSize;
}
    
bool HpccNanoPuArchtIngressPipe::IngressPipe (Ptr<NetDevice> device, 
                                              Ptr<const Packet> p, 
                                              uint16_t protocol, 
                                              const Address &from)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU HPCC IngressPipe received: " << 
                cp->ToString ());
  
  NS_ASSERT_MSG (protocol==0x0800,
                 "HpccNanoPuArcht works only with IPv4 packets!");
    
  Ipv4Header ipHdr;
  cp->RemoveHeader (ipHdr);  
    
  NS_ASSERT_MSG(ipHdr.GetProtocol() == IntHeader::PROT_NUMBER,
                "This ingress pipeline only works for HPCC Transport "
                "which requires an INT header to be appended after the IPv4 header!");
    
  IntHeader intHdr;
  cp->RemoveHeader (intHdr);
    
  NS_ASSERT_MSG(intHdr.GetProtocol() == HpccHeader::PROT_NUMBER,
                "This ingress pipeline only works for HPCC Transport "
                "which requires an HPCC header to be appended after the INT header!");
    
  HpccHeader hpccHdr;
  cp->RemoveHeader (hpccHdr);
    
  uint16_t txMsgId = hpccHdr.GetTxMsgId ();
  uint16_t pktOffset = hpccHdr.GetPktOffset ();
  uint16_t msgLen = (uint16_t)hpccHdr.GetMsgSize (); // Msg Len is in pkts for nanoPU Archt
    
  uint8_t hdrFlag = hpccHdr.GetFlags ();
  if (hdrFlag & HpccHeader::Flags_t::DATA)
  {   
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC IngressPipe processing a DATA packet.");
      
    Ipv4Address srcIp = ipHdr.GetSource ();
    uint16_t srcPort = hpccHdr.GetSrcPort ();
    uint16_t dstPort = hpccHdr.GetDstPort ();
      
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
      
    m_pktgen->CtrlPktEvent (srcIp, srcPort, dstPort, txMsgId, ackNo, msgLen, intHdr);  
    
  }  
  else if (hdrFlag & HpccHeader::Flags_t::ACK)
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC IngressPipe processing an ACK packet.");
    
    if (m_ackNos.find(txMsgId) == m_ackNos.end())
    {
      // This is a new message
      m_credits[txMsgId] = m_initCredit;
      m_ackNos[txMsgId] = pktOffset;
      m_winSizes[txMsgId] = (uint32_t)m_initCredit * m_mtu;
      m_lastUpdateSeqs[txMsgId] = 0;
      m_incStages[txMsgId] = 0;
      m_utilizations[txMsgId] = 1.0;
      m_prevIntHdrs[txMsgId] = intHdr;
      // TODO: Note that we are initializing some state values here
      //       for new messages, but they are actually modified below.
    }
    else
    {
      // This is not a new message
      if (pktOffset > m_ackNos[txMsgId])
        m_ackNos[txMsgId] = pktOffset;
    }
      
    m_packetize->DeliveredEvent (txMsgId, msgLen, setBitMapUntil (m_ackNos[txMsgId]));
//     Simulator::Schedule (NanoSeconds(HPCC_INGRESS_PIPE_DELAY), 
//                          &NanoPuArchtPacketize::DeliveredEvent, 
//                          m_packetize, txMsgId, msgLen, setBitMapUntil (pktOffset));
      
    uint32_t newWinSizeBytes;
    if (pktOffset > m_lastUpdateSeqs[txMsgId])
    {
      newWinSizeBytes = this->ComputeWind (txMsgId, this->MeasureInflight (txMsgId, intHdr), true);
      m_lastUpdateSeqs[txMsgId] = m_credits[txMsgId]+1;
    }
    else
    {
      newWinSizeBytes = this->ComputeWind (txMsgId, this->MeasureInflight (txMsgId, intHdr), false);
    }
    
    uint16_t newCreditPkts = m_ackNos[txMsgId] + this->ComputeNumPkts (newWinSizeBytes);
    if (newCreditPkts > m_credits[txMsgId])
        m_credits[txMsgId] = newCreditPkts;
      
    int rtxPkt = -1;
    m_packetize->CreditToBtxEvent (txMsgId, rtxPkt, m_credits[txMsgId], m_credits[txMsgId],
                                   NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
                                   std::greater<int>());
//     Simulator::Schedule (NanoSeconds(HPCC_INGRESS_PIPE_DELAY), 
//                          &NanoPuArchtPacketize::CreditToBtxEvent, m_packetize, 
//                          txMsgId, rtxPkt, m_credits[txMsgId], m_credits[txMsgId],
//                          NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
//                          std::greater<int>());
      
    if (pktOffset >= msgLen)
    {
      NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                   " NanoPU HPCC IngressPipe clearing state for msg " <<
                   txMsgId << " because the msg is completed.");
      m_credits.erase(txMsgId);
      m_ackNos.erase(txMsgId);
      m_winSizes.erase(txMsgId);
      m_lastUpdateSeqs.erase(txMsgId);
      m_incStages.erase(txMsgId);
      m_utilizations.erase(txMsgId);
      m_prevIntHdrs.erase(txMsgId);
        
      // TODO: Erasing state values would be an expensive solution.
      //       Instead, a mechanism to determine whether the current
      //       state is valid or it belogs to an old (completed or 
      //       expired) message would be prefferable.
      //       Or, the dstIP, dstPort, and srcPort of the message can
      //       be stored as the state of the message inside the 
      //       pipeline. As long as these values match the ones inside
      //       the headers, this is not a new message. Otherwise the 
      //       current state is invalidated and new message state is
      //       initiated.
    }
    else
    {
      m_prevIntHdrs[txMsgId] = intHdr;
    }
  }
  else
  {
    NS_LOG_WARN(Simulator::Now ().GetNanoSeconds () << 
                " NanoPU HPCC IngressPipe received an unknown type (" <<
                hpccHdr.FlagsToString (hdrFlag) << ") of packet!");
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
                                  double baseRtt,
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
    
  m_ingresspipe = CreateObject<HpccNanoPuArchtIngressPipe> (m_reassemble, m_packetize,
                                                            m_pktgen, baseRtt, 
                                                            device->GetMtu (),
                                                            initialCredit, winAI,
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