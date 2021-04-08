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
#include "ns3/boolean.h"

#include "ns3/node.h"
#include "ns3/ipv4.h"
#include "ns3/data-rate.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/nanopu-archt.h"
#include "homatr-nanopu-transport.h"
#include "ns3/ipv4-header.h"
#include "ns3/homa-header.h"

namespace ns3 {
    
NS_LOG_COMPONENT_DEFINE ("HomatrNanoPuArcht");

NS_OBJECT_ENSURE_REGISTERED (HomatrNanoPuArcht);

/******************************************************************************/

TypeId HomatrNanoPuArchtPktGen::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomatrNanoPuArchtPktGen")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HomatrNanoPuArchtPktGen::HomatrNanoPuArchtPktGen (Ptr<HomatrNanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
}

HomatrNanoPuArchtPktGen::~HomatrNanoPuArchtPktGen ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HomatrNanoPuArchtPktGen::CtrlPktEvent (homatrNanoPuCtrlMeta_t ctrlMeta)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU HomaTr PktGen processing CtrlPktEvent." <<
               " (" << HomaHeader::FlagsToString (ctrlMeta.flag) << ")");
  
  Ptr<Packet> p = Create<Packet> ();
  HomaHeader homah;
    
  egressMeta_t egressMeta = {};
  egressMeta.containsData = false;
  egressMeta.isNewMsg = false;
  egressMeta.remoteIp = ctrlMeta.remoteIp;
  egressMeta.localPort = ctrlMeta.localPort;
  egressMeta.remotePort = ctrlMeta.remotePort;
  egressMeta.txMsgId = ctrlMeta.txMsgId;
  egressMeta.msgLen = ctrlMeta.msgLen;
  egressMeta.pktOffset = ctrlMeta.grantOffset; // For debugging purposes
  egressMeta.rank = 0; // High Rank for control packets
  
  homah.SetSrcPort (ctrlMeta.localPort);
  homah.SetDstPort (ctrlMeta.remotePort);
  homah.SetTxMsgId (ctrlMeta.txMsgId);
  homah.SetMsgSize (ctrlMeta.msgLen);
  homah.SetPktOffset (ctrlMeta.pktOffset);
  homah.SetGrantOffset (ctrlMeta.grantOffset);
  homah.SetPrio (ctrlMeta.priority);
  homah.SetPayloadSize (0);
  homah.SetFlags (ctrlMeta.flag);
  p-> AddHeader (homah);
    
  m_nanoPuArcht->GetArbiter ()->Receive(p, egressMeta);
}

/******************************************************************************/
 
TypeId HomatrNanoPuArchtIngressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomatrNanoPuArchtIngressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HomatrNanoPuArchtIngressPipe::HomatrNanoPuArchtIngressPipe (Ptr<HomatrNanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  m_nanoPuArcht = nanoPuArcht;
    
  uint8_t acceptedSchedOrder = m_nanoPuArcht->GetOvercommitLevel();
   
  Callback<bool, nanoPuArchtSchedObj_t<homatrSchedMeta_t>> homatrSchedPredicate;
  homatrSchedPredicate = MakeCallback (&HomatrNanoPuArchtIngressPipe::SchedPredicate, this);
    
  Callback<void, nanoPuArchtSchedObj_t<homatrSchedMeta_t>&> homatrPostSchedOp;
  homatrPostSchedOp = MakeCallback (&HomatrNanoPuArchtIngressPipe::PostSchedOp, this);
    
  m_nanoPuArchtScheduler = CreateObject<NanoPuArchtScheduler<homatrSchedMeta_t>>
                             (acceptedSchedOrder, homatrSchedPredicate, homatrPostSchedOp);
    
  m_nanoPuArchtScheduler->SetNumActiveMsgs(m_nanoPuArcht->GetNumActiveMsgsInSched());
}

HomatrNanoPuArchtIngressPipe::~HomatrNanoPuArchtIngressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
bool HomatrNanoPuArchtIngressPipe::SchedPredicate (nanoPuArchtSchedObj_t<homatrSchedMeta_t> obj) 
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << obj.id << obj.rank
                    << obj.metaData.grantableIdx << obj.metaData.grantedIdx);
  NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () << 
               " HomaTr SchedPredicate id: " << obj.id << " rank: " << obj.rank << " meta: " <<
               obj.metaData.grantableIdx << ", " << obj.metaData.grantedIdx);
    
  return obj.metaData.grantableIdx > obj.metaData.grantedIdx;
};
    
void HomatrNanoPuArchtIngressPipe::PostSchedOp (nanoPuArchtSchedObj_t<homatrSchedMeta_t>& obj)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << obj.id << obj.rank
                    << obj.metaData.grantableIdx << obj.metaData.grantedIdx);
    
  obj.metaData.grantedIdx = obj.metaData.grantableIdx;
    
  NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () << 
               " HomaTr PostSchedOp id: " << obj.id << " rank: " << obj.rank << " meta: " <<
               obj.metaData.grantableIdx << ", " << obj.metaData.grantedIdx);
    
}
    
bool HomatrNanoPuArchtIngressPipe::IngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                                             uint16_t protocol, const Address &from)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " NanoPU HomaTr IngressPipe received: " << 
                cp->ToString ());
  
  NS_ASSERT_MSG (protocol==0x0800,
                 "HomatrNanoPuArcht works only with IPv4 packets!");
    
  Ipv4Header iph;
  cp->RemoveHeader (iph); 
    
  NS_ASSERT_MSG(iph.GetProtocol() == HomaHeader::PROT_NUMBER,
                "This ingress pipeline only works for HomaTr Transport");
    
  HomaHeader homah;
  cp->RemoveHeader (homah);
    
  Ipv4Address srcIp = iph.GetSource ();
  uint16_t srcPort = homah.GetSrcPort ();
  uint16_t dstPort = homah.GetDstPort ();
  uint16_t txMsgId = homah.GetTxMsgId ();
  uint16_t pktOffset = homah.GetPktOffset ();
  uint16_t msgLen = homah.GetMsgSize ();
  uint8_t rxFlag = homah.GetFlags ();
    
  SocketIpTosTag priorityTag;
  p-> PeekPacketTag (priorityTag);
  uint8_t priority = priorityTag.GetTos();
  if (rxFlag & HomaHeader::Flags_t::DATA)  
    m_nanoPuArcht->DataRecvTrace(p, srcIp, iph.GetDestination(),
                                 srcPort, dstPort, txMsgId, 
                                 pktOffset, priority); 
  else
    m_nanoPuArcht->CtrlRecvTrace(p, srcIp, iph.GetDestination(),
                                 srcPort, dstPort, rxFlag, 
                                 homah.GetGrantOffset(), homah.GetPrio());
    
  if (rxFlag & HomaHeader::Flags_t::DATA ||
      rxFlag & HomaHeader::Flags_t::BUSY ||
      rxFlag & HomaHeader::Flags_t::CHOP)
  {   
    bool getRxMsgInfoReadOnly = rxFlag & HomaHeader::Flags_t::BUSY ||
                                rxFlag & HomaHeader::Flags_t::CHOP;
      
    rxMsgInfoMeta_t rxMsgInfo = m_nanoPuArcht->GetReassemblyBuffer ()
                                             ->GetRxMsgInfo (srcIp, srcPort, 
                                                 txMsgId, msgLen, pktOffset,
                                                 getRxMsgInfoReadOnly);
      
    if (!rxMsgInfo.success)
      return false;
      
    if (rxFlag & HomaHeader::Flags_t::BUSY)
    {
      m_busyMsgs[rxMsgInfo.rxMsgId] = true;
      return true;
    }
    m_busyMsgs[rxMsgInfo.rxMsgId] = false;
      
    if (!rxMsgInfo.isNewPkt)
    {
      NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << 
                   " NanoPU HomaTr IngressPipe dropping" <<
                   " a duplicate packet:" << " (" << iph << ")" << 
                   " (" << homah << ")");
      return true;
    }
      
    if (rxFlag & HomaHeader::Flags_t::DATA)
    {
      reassembleMeta_t metaData = { 
        .rxMsgId = rxMsgInfo.rxMsgId,
        .srcIp = srcIp,
        .srcPort = srcPort,
        .dstPort = dstPort,
        .txMsgId = txMsgId,
        .msgLen = msgLen,
        .pktOffset = pktOffset
      };
      m_nanoPuArcht->GetReassemblyBuffer ()->ProcessNewPacket (cp, metaData);
//     Simulator::Schedule (NanoSeconds(HOMATR_INGRESS_PIPE_DELAY), 
//                          &NanoPuArchtReassemble::ProcessNewPacket, 
//                          m_nanoPuArcht->GetReassemblyBuffer (),
//                          cp, metaData);
      
      NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << 
                  " NanoPU HomaTr received msg: " << rxMsgInfo.rxMsgId <<
                  " pktOffset: " << pktOffset << 
                  " prio: " << (uint16_t)priority);
      
      if (rxMsgInfo.ackNo >= msgLen)
      {
        homatrNanoPuCtrlMeta_t ctrlMeta = {
          .flag = HomaHeader::Flags_t::ACK,
          .remoteIp = srcIp,
          .remotePort = srcPort,
          .localPort = dstPort,
          .txMsgId = txMsgId,
          .msgLen = msgLen,
          .pktOffset = rxMsgInfo.ackNo,
          .grantOffset = msgLen,
          .priority = 0
        }; 
        m_nanoPuArcht->GetPktGen () ->CtrlPktEvent (ctrlMeta);
        
        // Clear the state of this message for simulation performance
        m_busyMsgs.erase(rxMsgInfo.rxMsgId);
        m_pendingMsgInfo.erase(rxMsgInfo.rxMsgId);
        
        NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << " NanoPU HomaTr ACKed msg: " << 
                    txMsgId << " pktOffset: " << rxMsgInfo.ackNo << " (Completed!)");
        
        return true;
      }
    }
        
    // Update the state of the current msg
    if (rxMsgInfo.isNewMsg)
    {        
      homatrNanoPuPendingMsg_t pendingInfo = {
        .remoteIp = srcIp,
        .remotePort = srcPort,
        .localPort = dstPort,
        .txMsgId = txMsgId,
        .msgLen = msgLen,
        .ackNo = rxMsgInfo.ackNo,
        .grantedIdx = m_nanoPuArcht->GetInitialCredit (),
        .grantableIdx = (uint16_t)(m_nanoPuArcht->GetInitialCredit ()+1),
        .remainingSize = (uint16_t)(msgLen-1)
      };
      m_pendingMsgInfo[rxMsgInfo.rxMsgId] = pendingInfo;
    }
    else
    {
      if (rxFlag & HomaHeader::Flags_t::DATA)
      {
        m_pendingMsgInfo[rxMsgInfo.rxMsgId].grantableIdx++;
        m_pendingMsgInfo[rxMsgInfo.rxMsgId].remainingSize--;
      }
      m_pendingMsgInfo[rxMsgInfo.rxMsgId].ackNo = rxMsgInfo.ackNo;
    }
      
    if (rxFlag & HomaHeader::Flags_t::CHOP)
    {
      homatrNanoPuCtrlMeta_t ctrlMeta = {
        .flag = HomaHeader::Flags_t::NACK,
        .remoteIp = srcIp,
        .remotePort = srcPort,
        .localPort = dstPort,
        .txMsgId = txMsgId,
        .msgLen = msgLen,
        .pktOffset = pktOffset, // Used to NACK a particular packet
        .grantOffset = rxMsgInfo.ackNo, // Used as ACK number
        .priority = 0 // Unused in NACK packets
      };
      m_nanoPuArcht->GetPktGen ()->CtrlPktEvent (ctrlMeta);
        
      NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << 
                  " NanoPU HomaTr NACKed msg: " << txMsgId << 
                  " pktOffset: " << rxMsgInfo.ackNo);
      return true;
    }
      
    bool msgIsFullyGranted = m_pendingMsgInfo[rxMsgInfo.rxMsgId].grantedIdx >= msgLen;
    homatrSchedMeta_t schedulerMeta = {
      .grantableIdx = m_pendingMsgInfo[rxMsgInfo.rxMsgId].grantableIdx,
      .grantedIdx = m_pendingMsgInfo[rxMsgInfo.rxMsgId].grantedIdx
    };
      
    NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << 
                " NanoPU HomaTr called Scheduler for id: " << rxMsgInfo.rxMsgId <<
                " rank: " << m_pendingMsgInfo[rxMsgInfo.rxMsgId].remainingSize << 
                " removeFlag: " << msgIsFullyGranted);
      
    // Choose a msg to GRANT and fire CtrlPktEvent with given priority
    bool schedulingIsSuccessful;
    uint16_t rxMsgIdToGrant;
    uint8_t prioBand;
    std::tie(schedulingIsSuccessful, rxMsgIdToGrant, prioBand) 
        = m_nanoPuArchtScheduler->UpdateAndSchedule (rxMsgInfo.rxMsgId,
                     m_pendingMsgInfo[rxMsgInfo.rxMsgId].remainingSize,
                                      msgIsFullyGranted, schedulerMeta);
    // TODO: If an inbound messages expires on Reassembly buffer, its entry 
    //       will still exist inside the scheduler. It will unnecessarily 
    //       occupy flip-flop structure.
      
    if (schedulingIsSuccessful && 
        prioBand < m_nanoPuArcht->GetOvercommitLevel() &&
        !m_busyMsgs[rxMsgIdToGrant])
    {   
      prioBand += m_nanoPuArcht->GetNumUnschedPrioBands();
      prioBand = std::min(prioBand, (uint8_t)(m_nanoPuArcht->GetNumTotalPrioBands()-1));
        
      homatrNanoPuCtrlMeta_t ctrlMeta = {
        .flag = HomaHeader::Flags_t::GRANT,
        .remoteIp = m_pendingMsgInfo[rxMsgIdToGrant].remoteIp,
        .remotePort = m_pendingMsgInfo[rxMsgIdToGrant].remotePort,
        .localPort = m_pendingMsgInfo[rxMsgIdToGrant].localPort,
        .txMsgId = m_pendingMsgInfo[rxMsgIdToGrant].txMsgId,
        .msgLen = m_pendingMsgInfo[rxMsgIdToGrant].msgLen,
        .pktOffset = m_pendingMsgInfo[rxMsgIdToGrant].ackNo,
        .grantOffset = m_pendingMsgInfo[rxMsgIdToGrant].grantableIdx,
        .priority = prioBand
      };
      m_nanoPuArcht->GetPktGen ()->CtrlPktEvent (ctrlMeta);
        
      m_pendingMsgInfo[rxMsgIdToGrant].grantedIdx = ctrlMeta.grantOffset;
        
      NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () <<  
                  " NanoPU HomaTr GRANTed msg: " << rxMsgIdToGrant <<
                  " grantOffset: " << m_pendingMsgInfo[rxMsgIdToGrant].grantedIdx << 
                  " with prio: " << (uint16_t)prioBand);
    }
    else // CAUTION: The block below creates extra control packets!
    {
      // NOTE: Homa doesn't explicitly acknowledge packets before the message completes.
      //       However NanoPU version does it when no new message can be granted 
      //       otherwise. This is important because the retransmission mechanism of
      //       NanoPU is not receiver based but sender based. Therefore we need to 
      //       minimize retransmissions by letting sender know which packets are 
      //       actually delivered.
      homatrNanoPuCtrlMeta_t ctrlMeta = {
        .flag = HomaHeader::Flags_t::ACK,
        .remoteIp = srcIp,
        .remotePort = srcPort,
        .localPort = dstPort,
        .txMsgId = txMsgId,
        .msgLen = msgLen,
        .pktOffset = rxMsgInfo.ackNo,
        .grantOffset = 0, // This is just an ACK, no packet is granted
        .priority = (uint8_t)(m_nanoPuArcht->GetNumTotalPrioBands()-1) // not used in ACKs
      };
      m_nanoPuArcht->GetPktGen ()->CtrlPktEvent (ctrlMeta);
        
      NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << " NanoPU Homa ACKed msg: " << 
                  rxMsgInfo.rxMsgId << " pktOffset: " << rxMsgInfo.ackNo);
    }
        
    // TODO: Homa keeps a timer per granted inbound messages and send
    //       a RESEND packet once the timer expires.
 
  }
  else if (rxFlag & HomaHeader::Flags_t::GRANT ||
           rxFlag & HomaHeader::Flags_t::RESEND ||
           rxFlag & HomaHeader::Flags_t::ACK)
  {
    NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HomaTr IngressPipe processing a "
                 << homah.FlagsToString(rxFlag) << " packet.");
    
    if (rxFlag & HomaHeader::Flags_t::ACK)
    {
      m_nanoPuArcht->GetPacketizationBuffer ()
                   ->DeliveredEvent (txMsgId, msgLen, setBitMapUntil(pktOffset));
    }
    else
    {
      int rtxPkt = -1;
      uint16_t credit = homah.GetGrantOffset ();
        
      homatrNanoPuCtrlMeta_t ctrlMeta = {
        .flag = HomaHeader::Flags_t::BOGUS,
        .remoteIp = srcIp,
        .remotePort = srcPort,
        .localPort = dstPort,
        .txMsgId = txMsgId,
        .msgLen = msgLen,
        .pktOffset = pktOffset,
        .grantOffset = credit,
        .priority = homah.GetPrio ()
      };
        
      if (rxFlag & HomaHeader::Flags_t::GRANT)
      {   
        m_nanoPuArcht->GetPacketizationBuffer ()
                     ->DeliveredEvent (txMsgId, msgLen, setBitMapUntil(pktOffset));
      }
      else if (rxFlag & HomaHeader::Flags_t::RESEND)
      {   
        // Generate a BUSY packet anyway. This packet will be dropped 
        // in the egress pipeline if the message is the active one.
        ctrlMeta.flag = HomaHeader::Flags_t::BUSY;
          
        rtxPkt = pktOffset;
      }
        
      m_nanoPuArcht->GetPktGen ()->CtrlPktEvent (ctrlMeta);
        
      m_nanoPuArcht->GetPacketizationBuffer ()
                   ->CreditToBtxEvent (txMsgId, rtxPkt, credit, credit,
                                       NanoPuArchtPacketize::CreditEventOpCode_t::WRITE,
                                       std::greater<int>());
    }
  }
  else if (rxFlag & HomaHeader::Flags_t::NACK)
  {
    uint16_t ackNo = homah.GetGrantOffset ();
    m_nanoPuArcht->GetPacketizationBuffer ()
                 ->DeliveredEvent (txMsgId, msgLen, setBitMapUntil(ackNo));
    m_nanoPuArcht->GetPacketizationBuffer ()
                 ->CreditToBtxEvent (txMsgId, pktOffset, -1, 0, 
                                     NanoPuArchtPacketize::CreditEventOpCode_t::NO_OP,
                                     std::greater<int>());
  }
  else
  {
    NS_LOG_WARN (Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HomaTr IngressPipe received an unknown ("
                 << homah.FlagsToString(rxFlag) << ") type of a packet.");
  }
    
  return true;
}
    
/******************************************************************************/
    
TypeId HomatrNanoPuArchtEgressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomatrNanoPuArchtEgressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

HomatrNanoPuArchtEgressPipe::HomatrNanoPuArchtEgressPipe (Ptr<HomatrNanoPuArcht> nanoPuArcht)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << nanoPuArcht);
    
  m_nanoPuArcht = nanoPuArcht;
}

HomatrNanoPuArchtEgressPipe::~HomatrNanoPuArchtEgressPipe ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
uint8_t HomatrNanoPuArchtEgressPipe::GetPriority (uint16_t msgLen)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  // TODO: Homa determines the priority of unscheduled packets
  //       based on the msgLen distribution

  if (msgLen <= m_nanoPuArcht->GetInitialCredit ())
    return 0;
  else
    return m_nanoPuArcht->GetNumUnschedPrioBands () -1;
}
    
void HomatrNanoPuArchtEgressPipe::EgressPipe (Ptr<const Packet> p, egressMeta_t meta)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << cp);
    
  HomaHeader homah;
  uint8_t priority;
  
  if (meta.containsData)
  {
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HomaTr EgressPipe processing data packet.");
      
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
    if (meta.isRtx && m_priorities[meta.txMsgId] < m_nanoPuArcht->GetNumUnschedPrioBands ())
      priority = m_nanoPuArcht->GetNumUnschedPrioBands (); 
    else
      priority = m_priorities[meta.txMsgId];
      
    cp-> AddHeader (homah);
      
    m_nanoPuArcht->DataSendTrace(cp, m_nanoPuArcht->GetLocalIp (), meta.remoteIp,
                                 meta.localPort, meta.remotePort, meta.txMsgId, 
                                 meta.pktOffset, meta.rank);
      
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
        
      if (ctrlFlag & HomaHeader::Flags_t::BOGUS || 
          (ctrlFlag & HomaHeader::Flags_t::BUSY &&
           m_activeOutboundMsgId == meta.txMsgId))
      {
        NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                     " NanoPU Homa EgressPipe dropping the " << 
                     homah.FlagsToString (ctrlFlag) << " packet.");
        m_nanoPuArcht->GetArbiter ()->EmitAfterPktOfSize (0);
        return;
      }
    }
      
    NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << 
                 " NanoPU HomaTr EgressPipe processing control packet.");
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
               " NanoPU HomaTr EgressPipe sending: " << 
                cp->ToString ());
    
  m_nanoPuArcht->GetArbiter ()->EmitAfterPktOfSize (cp->GetSize ());
    
  m_nanoPuArcht->SendToNetwork(cp);
//   Simulator::Schedule (NanoSeconds(HOMATR_EGRESS_PIPE_DELAY), 
//                        &NanoPuArcht::SendToNetwork, m_nanoPuArcht, cp);

  return;
}
    
/******************************************************************************/
       
TypeId HomatrNanoPuArcht::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomatrNanoPuArcht")
    .SetParent<Object> ()
    .SetGroupName("Network")
    .AddConstructor<HomatrNanoPuArcht> ()
    .AddAttribute ("PayloadSize", 
                   "MTU for the network interface excluding the header sizes",
                   UintegerValue (1400),
                   MakeUintegerAccessor (&HomatrNanoPuArcht::m_payloadSize),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxNMessages", 
                   "Maximum number of messages NanoPU can handle at a time",
                   UintegerValue (100),
                   MakeUintegerAccessor (&HomatrNanoPuArcht::m_maxNMessages),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TimeoutInterval", "Time value to expire the timers",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&HomatrNanoPuArcht::m_timeoutInterval),
                   MakeTimeChecker (MicroSeconds (0)))
    .AddAttribute ("InitialCredit", "Initial window of packets to be sent",
                   UintegerValue (10),
                   MakeUintegerAccessor (&HomatrNanoPuArcht::m_initialCredit),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MaxNTimeouts", 
                   "Max allowed number of retransmissions before discarding a msg",
                   UintegerValue (5),
                   MakeUintegerAccessor (&HomatrNanoPuArcht::m_maxTimeoutCnt),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("OptimizeMemory", 
                   "High performant mode (only packet sizes are stored to save from memory).",
                   BooleanValue (true),
                   MakeBooleanAccessor (&HomatrNanoPuArcht::m_memIsOptimized),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableArbiterQueueing", 
                   "Enables priority queuing on Arbiter.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&HomatrNanoPuArcht::m_enableArbiterQueueing),
                   MakeBooleanChecker ())
    .AddAttribute ("NumTotalPrioBands", "Total number of priority levels used within the network",
                   UintegerValue (8),
                   MakeUintegerAccessor (&HomatrNanoPuArcht::m_numTotalPrioBands),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("NumUnschedPrioBands", "Number of priority bands dedicated for unscheduled packets",
                   UintegerValue (2),
                   MakeUintegerAccessor (&HomatrNanoPuArcht::m_numUnschedPrioBands),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("OvercommitLevel", "Minimum number of messages to Grant at the same time",
                   UintegerValue (6),
                   MakeUintegerAccessor (&HomatrNanoPuArcht::m_overcommitLevel),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("NumActiveMsgsInSched", "Number of active messages to be configured in the scheduler extern",
                   UintegerValue (16),
                   MakeUintegerAccessor (&HomatrNanoPuArcht::m_nActiveMsgsInScheduler),
                   MakeUintegerChecker<uint8_t> ())
    .AddTraceSource ("MsgBegin",
                     "Trace source indicating a message has been delivered to "
                     "the the NanoPuArcht by the sender application layer.",
                     MakeTraceSourceAccessor (&HomatrNanoPuArcht::m_msgBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MsgFinish",
                     "Trace source indicating a message has been delivered to "
                     "the receiver application by the NanoPuArcht layer.",
                     MakeTraceSourceAccessor (&HomatrNanoPuArcht::m_msgFinishTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PacketsInArbiterQueue",
                     "Number of packets currently stored in the arbiter queue",
                     MakeTraceSourceAccessor (&HomatrNanoPuArcht::m_nArbiterPackets),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("BytesInArbiterQueue",
                     "Number of bytes (without metadata) currently stored in the arbiter queue",
                     MakeTraceSourceAccessor (&HomatrNanoPuArcht::m_nArbiterBytes),
                     "ns3::TracedValueCallback::Uint32")
    .AddTraceSource ("DataPktArrival",
                     "Trace source indicating a DATA packet has arrived "
                     "to the receiver HomatrNanoPuArcht layer.",
                     MakeTraceSourceAccessor (&HomatrNanoPuArcht::m_dataRecvTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("DataPktDeparture",
                     "Trace source indicating a DATA packet has departed "
                     "from the sender HomatrNanoPuArcht layer.",
                     MakeTraceSourceAccessor (&HomatrNanoPuArcht::m_dataSendTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("CtrlPktArrival",
                     "Trace source indicating a control packet has arrived "
                     "to the receiver HomatrNanoPuArcht layer.",
                     MakeTraceSourceAccessor (&HomatrNanoPuArcht::m_ctrlRecvTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}

HomatrNanoPuArcht::HomatrNanoPuArcht () : NanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}

HomatrNanoPuArcht::~HomatrNanoPuArcht ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
}
    
void HomatrNanoPuArcht::AggregateIntoDevice (Ptr<NetDevice> device)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << device); 
    
  NanoPuArcht::AggregateIntoDevice (device);
    
  m_pktgen = CreateObject<HomatrNanoPuArchtPktGen> (this);
    
  m_egresspipe = CreateObject<HomatrNanoPuArchtEgressPipe> (this);
    
  m_arbiter->SetEgressPipe (m_egresspipe);
    
  m_ingresspipe = CreateObject<HomatrNanoPuArchtIngressPipe> (this);
}
    
Ptr<HomatrNanoPuArchtPktGen> 
HomatrNanoPuArcht::GetPktGen (void)
{
  return m_pktgen;
}
    
uint8_t
HomatrNanoPuArcht::GetNumTotalPrioBands (void) const
{
  return m_numTotalPrioBands;
}
    
uint8_t
HomatrNanoPuArcht::GetNumUnschedPrioBands (void) const
{
  return m_numUnschedPrioBands;
}
    
uint8_t
HomatrNanoPuArcht::GetOvercommitLevel (void) const
{
  return m_overcommitLevel;
}
    
uint8_t
HomatrNanoPuArcht::GetNumActiveMsgsInSched (void) const
{
  return m_nActiveMsgsInScheduler;
}
    
bool HomatrNanoPuArcht::EnterIngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                                       uint16_t protocol, const Address &from)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
    
  m_ingresspipe->IngressPipe (device, p, protocol, from);
    
  return true;
}
    
void HomatrNanoPuArcht::DataRecvTrace (Ptr<const Packet> p, 
                                     Ipv4Address srcIp, Ipv4Address dstIp,
                                     uint16_t srcPort, uint16_t dstPort, 
                                     int txMsgId, uint16_t pktOffset, uint8_t prio)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
    
  m_dataRecvTrace(p, srcIp, dstIp, srcPort, dstPort, txMsgId, pktOffset, prio);
}
    
void HomatrNanoPuArcht::DataSendTrace (Ptr<const Packet> p, 
                                     Ipv4Address srcIp, Ipv4Address dstIp,
                                     uint16_t srcPort, uint16_t dstPort, 
                                     int txMsgId, uint16_t pktOffset, uint16_t prio)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
    
  m_dataSendTrace(p, srcIp, dstIp, srcPort, dstPort, txMsgId, pktOffset, prio);
}
    
void HomatrNanoPuArcht::CtrlRecvTrace (Ptr<const Packet> p, 
                                     Ipv4Address srcIp, Ipv4Address dstIp,
                                     uint16_t srcPort, uint16_t dstPort, 
                                     uint8_t flags, uint16_t grantOffset, uint8_t prio)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this << p);
    
  m_ctrlRecvTrace(p, srcIp, dstIp, srcPort, dstPort, flags, grantOffset, prio);
}
    
} // namespace ns3