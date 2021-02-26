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
#include "ns3/boolean.h"

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

HpccNanoPuArchtPktGen::HpccNanoPuArchtPktGen (Ptr<HpccNanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
}

HpccNanoPuArchtPktGen::~HpccNanoPuArchtPktGen ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HpccNanoPuArchtPktGen::CtrlPktEvent (hpccNanoPuCtrlMeta_t ctrlMeta,
                                          Ptr<Packet> p)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU HPCC PktGen processing CtrlPktEvent." <<
               " txMsgId: " << ctrlMeta.txMsgId << " pktOffset: " << ctrlMeta.ackNo);
    
  egressMeta_t meta = {};
  meta.containsData = false;
  meta.rank = 0; // High Rank for control packets
  meta.remoteIp = ctrlMeta.remoteIp;
  meta.remotePort = ctrlMeta.remotePort;
  meta.localPort = ctrlMeta.localPort;
    
  ctrlMeta.receivedIntHeader.SetProtocol(0); // Nothing else will exist after this in an ACK packet.
    
  HpccHeader hpcch;
  hpcch.SetSrcPort (ctrlMeta.localPort);
  hpcch.SetDstPort (ctrlMeta.remotePort);
  hpcch.SetTxMsgId (ctrlMeta.txMsgId);
  hpcch.SetFlags (HpccHeader::Flags_t::ACK);
  hpcch.SetPktOffset (ctrlMeta.ackNo);
  hpcch.SetMsgSize (ctrlMeta.msgLen);
  hpcch.SetPayloadSize ((uint16_t)ctrlMeta.receivedIntHeader.GetSerializedSize());
    
  if (m_nanoPuArcht->MemIsOptimized ())
  { // Avoid creating a new packet
    p->RemoveAtEnd (p->GetSize()); // This is a non-dirty operation that should be cheap
    p->AddHeader (ctrlMeta.receivedIntHeader); // This will be payload of HPCC header.
    p->AddHeader (hpcch);
      
    NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC PktGen generated: " << p->ToString ());
      
    m_nanoPuArcht->GetArbiter ()->Receive(p, meta);
  }
  else
  {
    Ptr<Packet> cp = Create<Packet> ();
    cp->AddHeader (ctrlMeta.receivedIntHeader); // This will be payload of HPCC header.
    cp->AddHeader (hpcch);
      
    NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC PktGen generated: " << cp->ToString ());
      
    m_nanoPuArcht->GetArbiter ()->Receive(cp, meta);
  } 
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

HpccNanoPuArchtIngressPipe::HpccNanoPuArchtIngressPipe (Ptr<HpccNanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
    
  m_maxWinSize = m_nanoPuArcht->GetInitialCredit () 
                  * m_nanoPuArcht->GetPayloadSize ();
}

HpccNanoPuArchtIngressPipe::~HpccNanoPuArchtIngressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
uint16_t HpccNanoPuArchtIngressPipe::ComputeNumPkts (uint32_t bytes)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << bytes);
    
  // TODO: The division operation below can be handled with
  //       a lookup table in a programmable hardware pipeline.
  uint32_t mtu = m_nanoPuArcht->GetPayloadSize ();
  uint32_t result = bytes / mtu + (bytes % mtu != 0);
    
  NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << 
              " ComputeNumPkts: " << bytes << 
              " Bytes -> " << result << " packets.");
  return result;
}
    
void HpccNanoPuArchtIngressPipe::MeasureInflight (uint16_t txMsgId, 
                                                    IntHeader intHdr)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  // TODO: A P4 programmable pipeline wouldn't have double type variables.
  //       Maybe we can represent utilization as an integer from 0 to 100?
    
  // TODO: The code below uses cascaded if statements to prevent the use
  //       of for loop. This design assumes a particular MAX_INT_HOPS value.
    
  // TODO: The code below uses relatively complex computations. We could 
  //       probably use lookup tables for such computations?
    
  double baseRtt = m_nanoPuArcht->GetBaseRtt ();
    
  uint8_t nHops = intHdr.GetNHops ();
  intHop_t curHopInfo;
  intHop_t oldHopInfo;
  double maxUtil = 0.0;
  double curUtil;
  double tao = 0.0;
  double curTao;
  double txRate;
  
  // TODO: Unroll the for loop below when implementing on P4
  for (uint8_t curHopIdx = 0; curHopIdx < nHops; curHopIdx++)
  { // Ignore the first hop which is essentially the bursty host itself
    curHopInfo = intHdr.PeekHopN (curHopIdx);
    oldHopInfo = m_msgStates[txMsgId].prevIntHeader.PeekHopN (curHopIdx);
      
    NS_ASSERT_MSG (oldHopInfo.bitRate != 0, 
                   "The old INT header for a message has fewer "
                   "hops than the current INT header.");
      
    uint64_t timeDelta;
    if (curHopInfo.time > oldHopInfo.time)
        timeDelta = curHopInfo.time - oldHopInfo.time;
    else
        timeDelta = curHopInfo.time + (1<<IntHeader::TIME_WIDTH) - oldHopInfo.time;
    curTao = ((double)timeDelta) *1e-9; //Converted to seconds
      
    uint32_t bytesDelta;
    if (curHopInfo.txBytes >= oldHopInfo.txBytes)
      bytesDelta = curHopInfo.txBytes - oldHopInfo.txBytes;
    else
      bytesDelta = curHopInfo.txBytes + (1<<IntHeader::BYTE_WIDTH) - oldHopInfo.txBytes;
    txRate = ((double)bytesDelta) * 8.0 / curTao;
      
    curUtil = (double)std::min(curHopInfo.qlen, oldHopInfo.qlen) 
              * (double)(m_nanoPuArcht->GetNicRate().GetBitRate())
              / ((double)curHopInfo.bitRate * m_maxWinSize);
    curUtil += txRate / (double)curHopInfo.bitRate;
      
    NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
                 " maxUtil (" << maxUtil << 
                   ") to be updated to " << curUtil <<
                   " (curTao: " << curTao <<
                   " bytesDelta: " << bytesDelta << 
                   " txRate: " << txRate <<
                   " qlen: " << std::min(curHopInfo.qlen, oldHopInfo.qlen) <<
                   " m_maxWinSize: " << m_maxWinSize << ")");
      
    if (curUtil > maxUtil)
    {
      maxUtil = curUtil;
      tao = curTao;
    }
  }
    
  m_msgStates[txMsgId].prevIntHeader = intHdr;
    
  tao = std::min(tao, baseRtt);
  m_msgStates[txMsgId].util = (m_msgStates[txMsgId].util * (baseRtt - tao) + maxUtil * tao) / baseRtt;
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " MeasureInflight computed new utilization as: " <<
                m_msgStates[txMsgId].util << " (maxUtil: " << maxUtil << ")");
    
  return;
}
    
uint32_t HpccNanoPuArchtIngressPipe::ComputeWind (uint16_t txMsgId, 
                                                  bool fastReact)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () 
                   << this << txMsgId << fastReact);
    
  uint32_t newWinSize;
  uint16_t newIncStage;
    
  uint32_t winAi = m_nanoPuArcht->GetWinAi ();
  double targetUtil = m_nanoPuArcht->GetUtilFac ();
    
  double utilRatio = m_msgStates[txMsgId].util / targetUtil;
  if (utilRatio >= 1 || m_msgStates[txMsgId].incStage >= m_nanoPuArcht->GetMaxStage ())
  {
    newWinSize = m_msgStates[txMsgId].curWinSize / utilRatio + winAi;
    newIncStage = 0;
  }
  else
  {
    newWinSize = m_msgStates[txMsgId].curWinSize + winAi;
    newIncStage = m_msgStates[txMsgId].incStage + 1;
  }
   
  uint32_t minWinSize = m_nanoPuArcht->GetMinCredit () * m_nanoPuArcht->GetPayloadSize ();
  if (newWinSize < minWinSize)
    newWinSize = minWinSize;
  if (newWinSize > m_maxWinSize)
    newWinSize = m_maxWinSize;
    
  if (!fastReact)
  {
    m_msgStates[txMsgId].curWinSize = newWinSize;
    m_msgStates[txMsgId].incStage = newIncStage;
  }
  
  NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () <<
               " ComputeWind returns winSize: " << newWinSize <<
               " (utilization: " << m_msgStates[txMsgId].util << 
               " incStage: " << m_msgStates[txMsgId].incStage << ")");
    
  return newWinSize;
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
  NS_ASSERT(msgLen <= BITMAP_SIZE);
    
  uint8_t hdrFlag = hpccHdr.GetFlags ();
  if (hdrFlag & HpccHeader::Flags_t::DATA)
  {   
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC IngressPipe processing a DATA packet.");
      
    Ipv4Address srcIp = ipHdr.GetSource ();
    uint16_t srcPort = hpccHdr.GetSrcPort ();
    uint16_t dstPort = hpccHdr.GetDstPort ();
      
    rxMsgInfoMeta_t rxMsgInfo = m_nanoPuArcht->GetReassemblyBuffer ()
                                             ->GetRxMsgInfo (srcIp, 
                                                             srcPort, 
                                                             txMsgId,
                                                             msgLen, 
                                                             pktOffset);
    if (!rxMsgInfo.success)
      return false;
    
    uint16_t ackNo = rxMsgInfo.ackNo;
      
    if (rxMsgInfo.isNewPkt)
    {
      reassembleMeta_t metaData = {};
      metaData.rxMsgId = rxMsgInfo.rxMsgId;
      metaData.srcIp = srcIp;
      metaData.srcPort = srcPort;
      metaData.dstPort = dstPort;
      metaData.txMsgId = txMsgId;
      metaData.msgLen = msgLen;
      metaData.pktOffset = pktOffset;

      m_nanoPuArcht->GetReassemblyBuffer ()->ProcessNewPacket (cp, metaData);
    }
      
    hpccNanoPuCtrlMeta_t ctrlMeta = {};
    ctrlMeta.remoteIp = srcIp;
    ctrlMeta.remotePort = srcPort;
    ctrlMeta.localPort = dstPort;
    ctrlMeta.txMsgId = txMsgId;
    ctrlMeta.ackNo = ackNo;
    ctrlMeta.msgLen = msgLen;
    ctrlMeta.receivedIntHeader = intHdr;
      
    m_nanoPuArcht->GetPktGen ()->CtrlPktEvent (ctrlMeta, cp);
  }  
  else if (hdrFlag & HpccHeader::Flags_t::ACK)
  {
    NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC IngressPipe processing an ACK packet.");
      
    IntHeader intHdrOfData;
    cp->RemoveHeader (intHdrOfData);
    
    if (m_msgStates.find(txMsgId) == m_msgStates.end())
    {
      // This is a new message
      NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << 
                  " NanoPU HPCC IngressPipe is creating a new message state!");
        
      m_msgStates[txMsgId].credit = m_nanoPuArcht->GetInitialCredit ();
      m_msgStates[txMsgId].ackNo = pktOffset;
      m_msgStates[txMsgId].curWinSize = m_maxWinSize;
      m_msgStates[txMsgId].lastUpdateSeq = 0;
      m_msgStates[txMsgId].incStage = 0;
      m_msgStates[txMsgId].util = 1.0; // This is how it starts in the original code
      m_msgStates[txMsgId].prevIntHeader = intHdrOfData;
      m_msgStates[txMsgId].nDupAck = 0;
      // TODO: Note that we are initializing some state values here
      //       for new messages, but they are actually modified below.
    }
    else
    {
      // This is not a new message
      if (pktOffset > m_msgStates[txMsgId].ackNo)
      {
        m_msgStates[txMsgId].ackNo = pktOffset;
        m_msgStates[txMsgId].nDupAck = 0;
      }
      else if (pktOffset == m_msgStates[txMsgId].ackNo)
        m_msgStates[txMsgId].nDupAck++;
    }
      
    m_nanoPuArcht->GetPacketizationBuffer ()
                 ->DeliveredEvent (txMsgId, msgLen, 
                                   setBitMapUntil (m_msgStates[txMsgId].ackNo));
//     Simulator::Schedule (NanoSeconds(HPCC_INGRESS_PIPE_DELAY), 
//                          &NanoPuArchtPacketize::DeliveredEvent, 
//                          m_nanoPuArcht->GetPacketizationBuffer (), 
//                          txMsgId, msgLen, setBitMapUntil (m_msgStates[txMsgId].ackNo));
      
    if (m_msgStates[txMsgId].ackNo >= msgLen)
    {
      NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                   " NanoPU HPCC IngressPipe clearing state for msg " <<
                   txMsgId << " because the msg is completed.");
      m_msgStates.erase(txMsgId);
        
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
      return true;
    }
      
    uint16_t nextSeq = m_msgStates[txMsgId].credit + 1;
    if (m_msgStates[txMsgId].lastUpdateSeq == 0) // first RTT
    {
      NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
                   " Credit (" << m_msgStates[txMsgId].credit << 
                   ") to be updated to " << nextSeq <<
                   " (ackNo: " << m_msgStates[txMsgId].ackNo <<
                   " curWinSize: " << m_msgStates[txMsgId].curWinSize << 
                   " pktOffset: " << pktOffset << ")");
        
      m_msgStates[txMsgId].lastUpdateSeq = nextSeq;
      m_msgStates[txMsgId].credit = nextSeq;
    }
    else
    {
      bool fastReact = pktOffset <= m_msgStates[txMsgId].lastUpdateSeq;
      this->MeasureInflight (txMsgId, intHdrOfData);
                
      uint32_t winSizeBytes = this->ComputeWind (txMsgId, fastReact);
      uint16_t winSizePkts = this->ComputeNumPkts (winSizeBytes);
      winSizePkts += m_msgStates[txMsgId].nDupAck;
    
      uint16_t newCreditPkts = std::min((m_msgStates[txMsgId].ackNo + winSizePkts),
                                        (uint16_t)BITMAP_SIZE);
      
      NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
                   " Credit (" << m_msgStates[txMsgId].credit << 
                   ") to be updated to " << newCreditPkts <<
                   " (ackNo: " << m_msgStates[txMsgId].ackNo <<
                   " curWinSize: " << m_msgStates[txMsgId].curWinSize << 
                   " activeWinSizeBytes: " << winSizeBytes <<
                   " activeWinSizePkts: " << winSizePkts <<
                   " pktOffset: " << pktOffset << ")");
      
      if (newCreditPkts > m_msgStates[txMsgId].credit)
        m_msgStates[txMsgId].credit = newCreditPkts;
        
      if (!fastReact && nextSeq > m_msgStates[txMsgId].lastUpdateSeq)
          m_msgStates[txMsgId].lastUpdateSeq = nextSeq;
    }
      
    int rtxPkt = -1;
    m_nanoPuArcht->GetPacketizationBuffer ()
                 ->CreditToBtxEvent (txMsgId, rtxPkt, 
                                     m_msgStates[txMsgId].credit, m_msgStates[txMsgId].credit,
                                     NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
                                     std::greater<int>());
//     Simulator::Schedule (NanoSeconds(HPCC_INGRESS_PIPE_DELAY), 
//                          &NanoPuArchtPacketize::CreditToBtxEvent, 
//                          m_nanoPuArcht->GetPacketizationBuffer (), txMsgId, rtxPkt, 
//                          m_msgStates[txMsgId].credit, m_msgStates[txMsgId].credit,
//                          NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
//                          std::greater<int>());
  }
  else
  {
    NS_LOG_ERROR (Simulator::Now ().GetNanoSeconds () << 
                  " ERROR: NanoPU HPCC IngressPipe received an unknown type (" <<
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

HpccNanoPuArchtEgressPipe::HpccNanoPuArchtEgressPipe (Ptr<HpccNanoPuArcht> nanoPuArcht)
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
  
  if (meta.containsData)
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HPCC EgressPipe processing data packet.");
        
    HpccHeader hpcch;
    hpcch.SetSrcPort (meta.localPort);
    hpcch.SetDstPort (meta.remotePort);
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
  
//   Ptr<Node> node = m_nanoPuArcht->GetNode ();
//   Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4> ();
//   int32_t ifIndex = ipv4proto->GetInterfaceForDevice (boundnetdevice);
//   Ipv4Address srcIP = ipv4proto->SourceAddressSelection (ifIndex, meta.dstIP);
  Ipv4Address srcIP = m_nanoPuArcht->GetLocalIp ();
    
  Ipv4Header iph;
  iph.SetSource (srcIP);
  iph.SetDestination (meta.remoteIp);
  iph.SetPayloadSize (cp->GetSize ());
  iph.SetTtl (64);
  iph.SetProtocol (IntHeader::PROT_NUMBER);
  cp-> AddHeader (iph);
  
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
                " NanoPU HPCC EgressPipe sending: " << 
                cp->ToString ());
    
  m_nanoPuArcht->GetArbiter ()->EmitAfterPktOfSize (cp->GetSize ());
    
  m_nanoPuArcht->SendToNetwork(cp);
//   Simulator::Schedule (NanoSeconds(HPCC_EGRESS_PIPE_DELAY), 
//                        &NanoPuArcht::SendToNetwork, m_nanoPuArcht, cp);

  return;
}
    
/******************************************************************************/
       
TypeId HpccNanoPuArcht::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HpccNanoPuArcht")
    .SetParent<Object> ()
    .SetGroupName("Network")
    .AddConstructor<HpccNanoPuArcht> ()
    .AddAttribute ("PayloadSize", 
                   "MTU for the network interface excluding the header sizes",
                   UintegerValue (1400),
                   MakeUintegerAccessor (&HpccNanoPuArcht::m_payloadSize),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxNMessages", 
                   "Maximum number of messages NanoPU can handle at a time",
                   UintegerValue (100),
                   MakeUintegerAccessor (&HpccNanoPuArcht::m_maxNMessages),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TimeoutInterval", "Time value to expire the timers",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&HpccNanoPuArcht::m_timeoutInterval),
                   MakeTimeChecker (MicroSeconds (0)))
    .AddAttribute ("InitialCredit", "Initial window of packets to be sent",
                   UintegerValue (10),
                   MakeUintegerAccessor (&HpccNanoPuArcht::m_initialCredit),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxNTimeouts", 
                   "Max allowed number of retransmissions before discarding a msg",
                   UintegerValue (5),
                   MakeUintegerAccessor (&HpccNanoPuArcht::m_maxTimeoutCnt),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("OptimizeMemory", 
                   "High performant mode (only packet sizes are stored to save from memory).",
                   BooleanValue (true),
                   MakeBooleanAccessor (&HpccNanoPuArcht::m_memIsOptimized),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableArbiterQueueing", 
                   "Enables priority queuing on Arbiter.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&HpccNanoPuArcht::m_enableArbiterQueueing),
                   MakeBooleanChecker ())
    .AddAttribute ("BaseRTT", "The base propagation RTT in seconds.",
                   DoubleValue (MicroSeconds (13).GetSeconds ()),
                   MakeDoubleAccessor (&HpccNanoPuArcht::m_baseRtt),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("WinAI", "Additive increase factor in Bytes",
                   UintegerValue (80),
                   MakeUintegerAccessor (&HpccNanoPuArcht::m_winAi),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("UtilFactor", "Utilization Factor (defined as \eta in HPCC paper)",
                   DoubleValue (0.95),
                   MakeDoubleAccessor (&HpccNanoPuArcht::m_utilFac),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxStage", 
                   "Maximum number of stages before window is updated wrt. utilization",
                   UintegerValue (5),
                   MakeUintegerAccessor (&HpccNanoPuArcht::m_maxStage),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MinCredit", 
                   "Minimum number of packets to keep in flight",
                   UintegerValue (2),
                   MakeUintegerAccessor (&HpccNanoPuArcht::m_minCredit),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("MsgBegin",
                     "Trace source indicating a message has been delivered to "
                     "the the NanoPuArcht by the sender application layer.",
                     MakeTraceSourceAccessor (&HpccNanoPuArcht::m_msgBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MsgFinish",
                     "Trace source indicating a message has been delivered to "
                     "the receiver application by the NanoPuArcht layer.",
                     MakeTraceSourceAccessor (&HpccNanoPuArcht::m_msgFinishTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PacketsInArbiterQueue",
                     "Number of packets currently stored in the arbiter queue",
                     MakeTraceSourceAccessor (&HpccNanoPuArcht::m_nArbiterPackets),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("BytesInArbiterQueue",
                     "Number of bytes (without metadata) currently stored in the arbiter queue",
                     MakeTraceSourceAccessor (&HpccNanoPuArcht::m_nArbiterBytes),
                     "ns3::TracedValueCallback::Uint32")
  ;
  return tid;
}

HpccNanoPuArcht::HpccNanoPuArcht () : NanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}

HpccNanoPuArcht::~HpccNanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HpccNanoPuArcht::AggregateIntoDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << device); 
    
  NanoPuArcht::AggregateIntoDevice (device);
    
  m_pktgen = CreateObject<HpccNanoPuArchtPktGen> (this);
    
  m_egresspipe = CreateObject<HpccNanoPuArchtEgressPipe> (this);
    
  m_arbiter->SetEgressPipe (m_egresspipe);
    
  m_ingresspipe = CreateObject<HpccNanoPuArchtIngressPipe> (this);
}
    
Ptr<HpccNanoPuArchtPktGen> 
HpccNanoPuArcht::GetPktGen (void)
{
  return m_pktgen;
}
    
double HpccNanoPuArcht::GetBaseRtt (void)
{
  return m_baseRtt;
}
  
uint32_t HpccNanoPuArcht::GetWinAi (void)
{
  return m_winAi;
}
  
double HpccNanoPuArcht::GetUtilFac (void)
{
  return m_utilFac;
}
  
uint32_t HpccNanoPuArcht::GetMaxStage (void)
{
  return m_maxStage;
}
    
uint16_t HpccNanoPuArcht::GetMinCredit (void)
{
  return m_minCredit;
}
    
bool HpccNanoPuArcht::EnterIngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                                        uint16_t protocol, const Address &from)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
    
  m_ingresspipe->IngressPipe (device, p, protocol, from);
    
  return true;
}
    
} // namespace ns3