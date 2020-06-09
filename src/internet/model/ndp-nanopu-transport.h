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

#ifndef NDP_NANOPU_TRANSPORT_H
#define NDP_NANOPU_TRANSPORT_H

#include <unordered_map>

#include "ns3/object.h"
#include "ns3/nanopu-archt.h"

namespace ns3 {
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Ingress Pipeline Architecture for NanoPU with NDP Transport
 *
 */
class NdpNanoPuArchtIngressPipe : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NdpNanoPuArchtIngressPipe (Ptr<NanoPuArchtReassemble> reassemble);
  ~NdpNanoPuArchtIngressPipe (void);
  
  bool IngressPipe (Ptr<NetDevice> device, Ptr<const Packet> p, 
                    uint16_t protocol, const Address &from);
  
protected:

    Ptr<NanoPuArchtReassemble> m_reassemble; //!< the reassembly buffer of the architecture

    std::unordered_map<uint16_t, uint16_t> m_credits; //!< State to track credit for each msg {rx_msg_id => credit}
};
 
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Egress Pipeline Architecture for NanoPU with NDP Transport
 *
 */
class NdpNanoPuArchtEgressPipe : public NanoPuArchtEgressPipe
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NdpNanoPuArchtEgressPipe (Ptr<NanoPuArcht> nanoPuArcht);
  ~NdpNanoPuArchtEgressPipe (void);
  
  bool EgressPipe (Ptr<const Packet> p, egressMeta_t meta);
  
protected:
  Ptr<NanoPuArcht> m_nanoPuArcht; //!< the archt itself to be able to send packets
};
 
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Egress Pipeline Architecture for NanoPU with NDP Transport
 *
 */
class NdpNanoPuArchtPktGen : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NdpNanoPuArchtPktGen (Ptr<NanoPuArchtArbiter> arbiter);
  ~NdpNanoPuArchtPktGen (void);
  
protected:
  Ptr<NanoPuArchtArbiter> m_arbiter; //!< the archt itself to be able to send packets
};
 
/******************************************************************************/
 
/**
 * \ingroup nanopu-archt
 *
 * \brief Transport Specific Architecture for devices to replace internet and transport layers.
 *        This version of the architecture implements specifically the NDP transport protocol.
 *
 */
class NdpNanoPuArcht : public NanoPuArcht
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  
  NdpNanoPuArcht (Ptr<Node> node, uint16_t maxMessages=100);
  virtual ~NdpNanoPuArcht (void);
  
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

  Ptr<NdpNanoPuArchtIngressPipe> m_ingresspipe; //!< the programmable ingress pipeline for the archt
  Ptr<NdpNanoPuArchtEgressPipe> m_egresspipe; //!< the programmable egress pipeline for the archt
  Ptr<NdpNanoPuArchtPktGen> m_pktgen; //!< the programmable packet generator for the archt
};   

} // namespace ns3

#endif /* NDP_NANOPU_TRANSPORT */