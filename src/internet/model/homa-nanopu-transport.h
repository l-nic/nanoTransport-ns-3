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
#include <deque>
#include <array>

#include "ns3/object.h"
#include "ns3/nanopu-archt.h"

// Define module delay in nano seconds
#define INGRESS_PIPE_DELAY 5
#define EGRESS_PIPE_DELAY 1

namespace ns3 {

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

  HomaNanoPuArchtPktGen (Ptr<NanoPuArcht> nanoPuArcht);
  ~HomaNanoPuArchtPktGen (void);
  
  void CtrlPktEvent (uint8_t flag, Ipv4Address dstIp, uint16_t dstPort, 
                     uint16_t srcPort, uint16_t txMsgId, uint16_t msgLen, 
                     uint16_t pktOffset, uint16_t grantOffset, uint8_t priority);
  
protected:
  Ptr<NanoPuArcht> m_nanoPuArcht; //!< the archt itself to be able to configure pacer
  
  Time m_pacerLastTxTime; //!< The last simulation time the packet generator sent out a packet
  Time m_packetTxTime; //!< Time to transmit/receive a full MTU packet to/from the network
};
 
/******************************************************************************/

typedef struct scheduledMsgMeta_t {
    uint16_t rxMsgId;
    Ipv4Address srcIp;
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t txMsgId;
    uint16_t msgLen;
    uint16_t pktOffset;
    uint16_t grantOffset;
    uint8_t priority;
}scheduledMsgMeta_t;
    
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

  HomaNanoPuArchtIngressPipe (Ptr<NanoPuArchtReassemble> reassemble,
                             Ptr<NanoPuArchtPacketize> packetize,
                             Ptr<HomaNanoPuArchtPktGen> pktgen,
                             uint16_t rttPkts);
  ~HomaNanoPuArchtIngressPipe (void);
  
  bool IngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                    uint16_t protocol, const Address &from);
  
protected:

  Ptr<NanoPuArchtReassemble> m_reassemble; //!< the reassembly buffer of the architecture
  Ptr<NanoPuArchtPacketize> m_packetize; //!< the packetization buffer of the architecture
  Ptr<HomaNanoPuArchtPktGen> m_pktgen; //!< the programmable packet generator of the Homa architecture
  uint16_t m_rttPkts; //!< Average BDP of the network (in packets)
    
  std::unordered_map<uint16_t, uint16_t> m_credits; //!< grantOffset state for each {rxMsgId => credit}
    
  uint16_t m_priorities[3] = {5, 25, 100};
  uint8_t GetPriority (uint16_t msgLen);
  
  std::array<std::deque<uint16_t>, 4> m_scheduledMsgs; //!< List of scheduled messages for every level of priorities
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

  HomaNanoPuArchtEgressPipe (Ptr<NanoPuArcht> nanoPuArcht);
  ~HomaNanoPuArchtEgressPipe (void);
  
  void EgressPipe (Ptr<const Packet> p, egressMeta_t meta);
  
protected:
  Ptr<NanoPuArcht> m_nanoPuArcht; //!< the archt itself to be able to send packets
  
  std::unordered_map<uint16_t, uint8_t> m_priorities; //!< priority state for each {txMsgId => prio}
  
  uint16_t m_priorities[3] = {5, 25, 100};
  uint8_t GetPriority (uint16_t msgLen);
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
  
  HomaNanoPuArcht (Ptr<Node> node,
                  Ptr<NetDevice> device,
                  Time timeoutInterval,
                  uint16_t maxMessages=100,
                  uint16_t payloadSize=1445,
                  uint16_t initialCredit=10,
                  uint16_t maxTimeoutCnt=5);
  virtual ~HomaNanoPuArcht (void);
  
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

protected:

  Ptr<HomaNanoPuArchtIngressPipe> m_ingresspipe; //!< the programmable ingress pipeline for the archt
  Ptr<HomaNanoPuArchtEgressPipe> m_egresspipe; //!< the programmable egress pipeline for the archt
  Ptr<HomaNanoPuArchtPktGen> m_pktgen; //!< the programmable packet generator for the archt
};   

} // namespace ns3

#endif /* HOMA_NANOPU_TRANSPORT */