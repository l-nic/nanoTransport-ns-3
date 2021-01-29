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

#ifndef HOMA_NANOPU_TRANSPORT_H
#define HOMA_NANOPU_TRANSPORT_H

#include <unordered_map>
#include <tuple>

#include "ns3/object.h"
#include "ns3/nanopu-archt.h"

// Define module delay in nano seconds
#define HOMA_INGRESS_PIPE_DELAY 5
#define HOMA_EGRESS_PIPE_DELAY 1

namespace ns3 {
    
class HomaNanoPuArcht;
    
typedef struct homaNanoPuCtrlMeta_t {
    bool shouldUpdateState;
    bool shouldGenCtrlPkt;
    uint8_t flag;
    Ipv4Address remoteIp;
    uint16_t remotePort;
    uint16_t localPort;
    uint16_t txMsgId;
    uint16_t msgLen;
    uint16_t pktOffset;
    uint16_t grantOffset;
    uint8_t priority;
}homaNanoPuCtrlMeta_t;
    
typedef struct homaNanoPuPendingMsg_t {
    Ipv4Address remoteIp;
    uint16_t remotePort;
    uint16_t localPort;
    uint16_t txMsgId;
    uint16_t msgLen;
    uint16_t ackNo;
    uint16_t grantedIdx;
    uint16_t grantableIdx;
    uint16_t remainingSize;
}homaNanoPuPendingMsg_t;
    
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Programmable Packet Generator Architecture for NanoPU with Homa Transport
 *
 */
class HomaNanoPuArchtPktGen : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  HomaNanoPuArchtPktGen (Ptr<HomaNanoPuArcht> nanoPuArcht);
  ~HomaNanoPuArchtPktGen (void);
  
  void CtrlPktEvent (homaNanoPuCtrlMeta_t ctrlMeta);
  
private:
  Ptr<HomaNanoPuArcht> m_nanoPuArcht; //!< the archt itself to be able to configure pacer
};
 
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Ingress Pipeline Architecture for NanoPU with Homa Transport
 *
 */
class HomaNanoPuArchtIngressPipe : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  HomaNanoPuArchtIngressPipe (Ptr<HomaNanoPuArcht> nanoPuArcht);
  ~HomaNanoPuArchtIngressPipe (void);
  
  bool IngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                    uint16_t protocol, const Address &from);
  
protected:
  /**
   * \brief Chooses a pending msg and determines a priority to GRANT it
   * \return the corresponding rxMsgId and the priority
   */
  std::tuple<uint16_t, uint8_t> GetNextMsgIdToGrant (uint16_t mostRecentRxMsgId);

private:

  Ptr<HomaNanoPuArcht> m_nanoPuArcht; //!< the archt itself
  
  std::unordered_map<uint16_t, bool> m_busyMsgs; //!< state to mark BUSY messages {rxMsgId => BUSY mark}
  std::unordered_map<uint16_t, homaNanoPuPendingMsg_t> m_pendingMsgInfo; //!< pending msg state {rxMsgId => pending msg info}
};
 
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Egress Pipeline Architecture for NanoPU with Homa Transport
 *
 */
class HomaNanoPuArchtEgressPipe : public NanoPuArchtEgressPipe
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  HomaNanoPuArchtEgressPipe (Ptr<HomaNanoPuArcht> nanoPuArcht);
  ~HomaNanoPuArchtEgressPipe (void);
  
  void EgressPipe (Ptr<const Packet> p, egressMeta_t meta);
  
protected:
  /**
   * \brief Returns the priority for sending the unscheduled packets
   * \return priority value
   */
  uint8_t GetPriority (uint16_t msgLen);
  
private:
  
  Ptr<HomaNanoPuArcht> m_nanoPuArcht; //!< the archt itself to be able to send packets
  
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
 *        This version of the architecture implements specifically the Homa transport protocol.
 *
 */
class HomaNanoPuArcht : public NanoPuArcht
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  
  HomaNanoPuArcht ();
  virtual ~HomaNanoPuArcht (void);
  
  void AggregateIntoDevice (Ptr<NetDevice> device);
  
  /**
   * \brief Returns architecture's Packet Generator.
   * 
   * \returns Pointer to the packet generator.
   */
  Ptr<HomaNanoPuArchtPktGen> GetPktGen (void);
  
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

private:

  Ptr<HomaNanoPuArchtIngressPipe> m_ingresspipe; //!< the programmable ingress pipeline for the archt
  Ptr<HomaNanoPuArchtEgressPipe> m_egresspipe; //!< the programmable egress pipeline for the archt
  Ptr<HomaNanoPuArchtPktGen> m_pktgen; //!< the programmable packet generator for the archt
  
  uint8_t m_numTotalPrioBands;   //!< Total number of priority levels used within the network
  uint8_t m_numUnschedPrioBands; //!< Number of priority bands dedicated for unscheduled packets
  uint8_t m_overcommitLevel;     //!< Minimum number of messages to Grant at the same time
};   

} // namespace ns3

#endif /* HOMA_NANOPU_TRANSPORT */