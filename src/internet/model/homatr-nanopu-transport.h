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

#ifndef HOMATR_NANOPU_TRANSPORT_H
#define HOMATR_NANOPU_TRANSPORT_H

#include <functional>
#include <unordered_map>
#include <tuple>

#include "ns3/object.h"
#include "ns3/nanopu-archt.h"

// Define module delay in nano seconds
#define HOMATR_INGRESS_PIPE_DELAY 5
#define HOMATR_EGRESS_PIPE_DELAY 1

namespace ns3 {
    
class HomatrNanoPuArcht;
    
typedef struct homatrNanoPuCtrlMeta_t {
    uint8_t flag;
    Ipv4Address remoteIp;
    uint16_t remotePort;
    uint16_t localPort;
    uint16_t txMsgId;
    uint16_t msgLen;
    uint16_t pktOffset;
    uint16_t grantOffset;
    uint8_t priority;
}homatrNanoPuCtrlMeta_t;
    
typedef struct homatrNanoPuPendingMsg_t {
    Ipv4Address remoteIp;
    uint16_t remotePort;
    uint16_t localPort;
    uint16_t txMsgId;
    uint16_t msgLen;
    uint16_t ackNo;
    uint16_t grantedIdx;
    uint16_t grantableIdx;
    uint16_t remainingSize;
}homatrNanoPuPendingMsg_t;
    
typedef struct homatrSchedMeta_t {
    uint16_t grantableIdx;
    uint16_t grantedIdx;
}homatrSchedMeta_t; 
    
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Programmable Packet Generator Architecture for NanoPU with HomaTr Transport
 *
 */
class HomatrNanoPuArchtPktGen : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  HomatrNanoPuArchtPktGen (Ptr<HomatrNanoPuArcht> nanoPuArcht);
  ~HomatrNanoPuArchtPktGen (void);
  
  void CtrlPktEvent (homatrNanoPuCtrlMeta_t ctrlMeta);
  
private:
  Ptr<HomatrNanoPuArcht> m_nanoPuArcht; //!< the archt itself to be able to configure pacer
};
 
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Ingress Pipeline Architecture for NanoPU with HomaTr Transport
 *
 */
class HomatrNanoPuArchtIngressPipe : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  HomatrNanoPuArchtIngressPipe (Ptr<HomatrNanoPuArcht> nanoPuArcht);
  ~HomatrNanoPuArchtIngressPipe (void);
  
  /**
   * \brief Performs the predicate check for the NanoPuArchtScheduler extern
   * \param obj The scheduled object on which the predicate is being evaluated
   * \return the result of the predicate check
   */
  bool SchedPredicate (nanoPuArchtSchedObj_t<homatrSchedMeta_t> obj);
  
  /**
   * \brief Performs the post scheduling operation for the NanoPuArchtScheduler extern
   * \param obj The scheduled object on which the operation should be performed
   */
  void PostSchedOp (nanoPuArchtSchedObj_t<homatrSchedMeta_t>& obj);

  bool IngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                    uint16_t protocol, const Address &from);
                    
private:

  Ptr<HomatrNanoPuArcht> m_nanoPuArcht; //!< the archt itself
  Ptr<NanoPuArchtScheduler<homatrSchedMeta_t>> m_nanoPuArchtScheduler; //!< the scheduler
  
  std::unordered_map<uint16_t, bool> m_busyMsgs; //!< state to mark BUSY messages {rxMsgId => BUSY mark}
  std::unordered_map<uint16_t, homatrNanoPuPendingMsg_t> m_pendingMsgInfo; //!< pending msg state {rxMsgId => pending msg info}
};
 
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Egress Pipeline Architecture for NanoPU with HomaTr Transport
 *
 */
class HomatrNanoPuArchtEgressPipe : public NanoPuArchtEgressPipe
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  HomatrNanoPuArchtEgressPipe (Ptr<HomatrNanoPuArcht> nanoPuArcht);
  ~HomatrNanoPuArchtEgressPipe (void);
  
  void EgressPipe (Ptr<const Packet> p, egressMeta_t meta);
  
protected:
  /**
   * \brief Returns the priority for sending the unscheduled packets
   * \return priority value
   */
  uint8_t GetPriority (uint16_t msgLen);
  
private:
  
  Ptr<HomatrNanoPuArcht> m_nanoPuArcht; //!< the archt itself to be able to send packets
  
  std::unordered_map<uint16_t, uint8_t> m_priorities; //!< priority state for each outbound msg {txMsgId => prio}
  
  // Ideally the packetization buffer should be sending the message with
  // the shortest remaining size and run to completion. Then the active outbound msg
  // ID would stay the same while the message is being transmitted. This can be 
  // compared against when generating BUSY packets.
  uint16_t m_activeOutboundMsgId; //!< The txMsgId of the most recent active message
};
 
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Transport Specific Architecture for devices to replace internet and transport layers.
 *        This version of the architecture implements specifically the HomaTr transport protocol.
 *
 */
class HomatrNanoPuArcht : public NanoPuArcht
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  
  HomatrNanoPuArcht ();
  virtual ~HomatrNanoPuArcht (void);
  
  void AggregateIntoDevice (Ptr<NetDevice> device);
  
  /**
   * \brief Returns architecture's Packet Generator.
   * 
   * \returns Pointer to the packet generator.
   */
  Ptr<HomatrNanoPuArchtPktGen> GetPktGen (void);
  
  /**
   * \brief Get total number of priority levels in the network
   * \return Total number of priority levels used within the network
   */
  uint8_t GetNumTotalPrioBands (void) const;
  
  /**
   * \brief Get number of priority levels dedicated to unscheduled packets in the network
   * \return Number of priority bands dedicated for unscheduled packets
   */
  uint8_t GetNumUnschedPrioBands (void) const;
  
  /**
   * \brief Get the configured number of messages to grant at the same time
   * \return Minimum number of messages to Grant at the same time
   */
  uint8_t GetOvercommitLevel (void) const;
  
  uint8_t GetNumActiveMsgsInSched (void) const;
  
  /**
   * \brief Implements programmable ingress pipeline architecture.
   *
   * \param device Pointer to NetDevice of desired interface
   * \param p Pointer to the arriving packet
   * \param protocol L3 protocol of the incomming packet (Can assume IPv4 for nanoPU)
   * \param from The L2 source address of the incoming packet 
   * \returns boolean to check successful completion of the packet processing
   */
  bool EnterIngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                    uint16_t protocol, const Address &from);
       
  /**
   * \brief Calls the Data packet arrival trace source
   */
  void DataRecvTrace (Ptr<const Packet> p, Ipv4Address srcIp, Ipv4Address dstIp,
                      uint16_t srcPort, uint16_t dstPort, int txMsgId, 
                      uint16_t pktOffset, uint8_t prio);
  /**
   * \brief Calls the Data packet departure trace source
   */
  void DataSendTrace (Ptr<const Packet> p, Ipv4Address srcIp, Ipv4Address dstIp,
                      uint16_t srcPort, uint16_t dstPort, int txMsgId, 
                      uint16_t pktOffset, uint16_t prio);
  
  /**
   * \brief Calls the Control packet arrival trace source
   */
  void CtrlRecvTrace (Ptr<const Packet> p, 
                      Ipv4Address srcIp, Ipv4Address dstIp,
                      uint16_t srcPort, uint16_t dstPort, 
                      uint8_t flags, uint16_t grantOffset, uint8_t prio);

private:

  Ptr<HomatrNanoPuArchtIngressPipe> m_ingresspipe; //!< the programmable ingress pipeline for the archt
  Ptr<HomatrNanoPuArchtEgressPipe> m_egresspipe; //!< the programmable egress pipeline for the archt
  Ptr<HomatrNanoPuArchtPktGen> m_pktgen; //!< the programmable packet generator for the archt
  
  uint8_t m_numTotalPrioBands;   //!< Total number of priority levels used within the network
  uint8_t m_numUnschedPrioBands; //!< Number of priority bands dedicated for unscheduled packets
  uint8_t m_overcommitLevel;     //!< Minimum number of messages to Grant at the same time
  
  uint8_t m_nActiveMsgsInScheduler; //!< Allows configurability for Scheduler experiments
  
  TracedCallback<Ptr<const Packet>, Ipv4Address, Ipv4Address, 
                 uint16_t, uint16_t, int, 
                 uint16_t, uint8_t> m_dataRecvTrace; //!< Trace of {pkt, srcIp, dstIp, srcPort, dstPort, txMsgId, pktOffset, prio} for arriving DATA packets
  TracedCallback<Ptr<const Packet>, Ipv4Address, Ipv4Address, 
                 uint16_t, uint16_t, int, 
                 uint16_t, uint16_t> m_dataSendTrace; //!< Trace of {pkt, srcIp, dstIp, srcPort, dstPort, txMsgId, pktOffset, prio} for departing DATA packets
  TracedCallback<Ptr<const Packet>, Ipv4Address, Ipv4Address, 
                 uint16_t, uint16_t, uint8_t, 
                 uint16_t, uint8_t> m_ctrlRecvTrace; //!< Trace of {pkt, srcIp, dstIp, srcPort, dstPort, txMsgId, grantOffset, prio} for arriving DATA packets
};   

} // namespace ns3

#endif /* HOMATR_NANOPU_TRANSPORT */