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
#ifndef NANOPU_ARCHT_H
#define NANOPU_ARCHT_H

#include <unordered_map>
#include <tuple>
#include <list>
#include <math.h>

#include "ns3/object.h"

// Note that bitmap_t is defined as uint64_t below
#define BITMAP_SIZE 64

namespace ns3 {
    
class Node;
    
typedef uint64_t bitmap_t ;
typedef struct reassembleMeta_t {
    uint16_t rxMsgId;
    Ipv4Address srcIP;
    uint16_t srcPort;
    uint16_t txMsgId;
    uint16_t msgLen;
    uint16_t pktOffset;
}reassembleMeta_t;
typedef struct rxMsgInfoMeta_t {
    uint16_t rxMsgId;
    uint16_t ackNo; //!< Next expected packet
    bool isNewMsg;
    bool isNewPkt;
    bool success;
}rxMsgInfoMeta_t;
typedef struct egressMeta_t {
    bool isData;
    Ipv4Address dstIP;
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t txMsgId;
    uint16_t msgLen;
    uint16_t pktOffset;
}egressMeta_t;

/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Base Egress Pipeline Architecture for NanoPU. 
 *        Every transport protocol should implement their own egress pipe inheriting from this
 *
 */
class NanoPuArchtEgressPipe : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArchtEgressPipe ();
  ~NanoPuArchtEgressPipe (void);
  
protected:
};
 
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Arbiter Architecture for NanoPU
 *
 */
class NanoPuArchtArbiter : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArchtArbiter (void);
  ~NanoPuArchtArbiter (void);
  
  void SetEgressPipe (Ptr<NanoPuArchtEgressPipe> egressPipe);
  
protected:
  
  Ptr<NanoPuArchtEgressPipe> m_egressPipe;
};
    
/******************************************************************************/
   
/**
 * \ingroup nanopu-archt
 *
 * \brief Packetization Block for NanoPU Architecture
 *
 */
class NanoPuArchtPacketize : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArchtPacketize (Ptr<NanoPuArchtArbiter> arbiter);
  ~NanoPuArchtPacketize (void);
  
protected:
  
  Ptr<NanoPuArchtArbiter> m_arbiter;
};
    
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Timer Module for NanoPU Architecture
 *
 */
class NanoPuArchtTimer : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArchtTimer (Ptr<NanoPuArchtPacketize> packetize);
  ~NanoPuArchtTimer (void);
  
protected:
  
  Ptr<NanoPuArchtPacketize> m_packetize;
};
    
/******************************************************************************/

/* The rxMsgIdTable in the Reassembly Buffer requires a lookup table with multiple
 * key values. Thus we define appropriate template functions below for the 
 * corresponding unordered map below.
 */
typedef std::tuple<uint32_t, uint16_t, uint16_t> rxMsgIdTableKey_t;
struct rxMsgIdTable_hash : public std::unary_function<rxMsgIdTableKey_t, std::size_t>
{
  std::size_t operator()(const rxMsgIdTableKey_t& k) const
    {
      return std::get<0>(k) ^ std::get<1>(k) ^ std::get<2>(k);
    }
};
struct rxMsgIdTable_key_equal : public std::binary_function<rxMsgIdTableKey_t, rxMsgIdTableKey_t, bool>
{
  bool operator()(const rxMsgIdTableKey_t& v0, const rxMsgIdTableKey_t& v1) const
    {
      return (
        std::get<0>(v0) == std::get<0>(v1) &&
        std::get<1>(v0) == std::get<1>(v1) &&
        std::get<2>(v0) == std::get<2>(v1)
      );
    }
};
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Ingress Pipeline Architecture for NanoPU with NDP Transport
 *
 */
class NanoPuArchtReassemble : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArchtReassemble (uint16_t maxMessages);
  ~NanoPuArchtReassemble (void);
  
  rxMsgInfoMeta_t GetRxMsgInfo (Ipv4Address srcIp, uint16_t srcPort, uint16_t txMsgId,
                                uint16_t msgLen, uint16_t pktOffset);
  
protected:

    std::list<uint16_t> m_rxMsgIdFreeList; //!< List of free RX msg IDs
    std::unordered_map<const rxMsgIdTableKey_t, uint16_t, rxMsgIdTable_hash, 
                       rxMsgIdTable_key_equal> m_rxMsgIdTable; //!< table that maps {src_ip, src_port, tx_msg_id => rx_msg_id}
    std::unordered_map<uint16_t, uint8_t*> m_buffers; //!< message reassembly buffers, {rx_msg_id => ["pkt_0_data", ..., "pkt_N_data"]}
    std::unordered_map<uint16_t, bitmap_t> m_receivedBitmap; //!< bitmap to determine when all pkts have arrived, {rx_msg_id => bitmap}
};
    
/******************************************************************************/
    
/**
 * \ingroup network
 * \defgroup nanopu-archt NanoPU Architecture
 */

/**
 * \ingroup nanopu-archt
 *
 * \brief Architecture for devices to replace internet and transport layers
 *
 */
class NanoPuArcht : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArcht (Ptr<Node> node, uint16_t m_maxMessages=100);
  ~NanoPuArcht (void);
  
  /**
   * \brief Return the node this architecture is associated with.
   * \returns the node
   */
  Ptr<Node> GetNode (void);
  
  /**
   * \brief Bind the architecture to specific device.
   *
   * This method corresponds to using setsockopt() SO_BINDTODEVICE
   * of real network or BSD sockets.   If set, this option will
   * force packets to leave the bound device regardless of the device that
   * IP routing would naturally choose.  In the receive direction, only
   * packets received from the bound interface will be delivered.
   *
   * This option has no particular relationship to binding sockets to
   * an address via Socket::Bind ().  It is possible to bind sockets to a 
   * specific IP address on the bound interface by calling both 
   * Socket::Bind (address) and Socket::BindToNetDevice (device), but it
   * is also possible to bind to mismatching device and address, even if
   * the socket can not receive any packets as a result.
   *
   * \param netdevice Pointer to NetDevice of desired interface
   * \returns nothing
   */
  virtual void BindToNetDevice (Ptr<NetDevice> netdevice);

  /**
   * \brief Returns architecture's bound NetDevice, if any.
   *
   * This method corresponds to using getsockopt() SO_BINDTODEVICE
   * of real network or BSD sockets.
   * 
   * 
   * \returns Pointer to interface.
   */
  Ptr<NetDevice> GetBoundNetDevice (void); 
  
  virtual bool Send (Ptr<Packet> p, const Address &dest);
  
  virtual bool EnterIngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                            uint16_t protocol, const Address &from);
  
protected:

    Ptr<Node>      m_node; //!< the node this architecture is located at.
    Ptr<NetDevice> m_boundnetdevice; //!< the device this architecture is bound to (might be null).
    
    uint16_t m_mtu; //!< equal to the mtu set on the m_boundnetdevice
    uint16_t m_maxMessages; //!< Max number of msg Reassembly and Packetize modules can handle at a time
    
    Ptr<NanoPuArchtReassemble> m_reassemble; //!< the reassembly buffer of the architecture
    Ptr<NanoPuArchtArbiter> m_arbiter; //!< the arbiter of the architecture
    Ptr<NanoPuArchtPacketize> m_packetize; //!< the packetization block of the architecture
    Ptr<NanoPuArchtTimer> m_timer; //!< the timer module of the architecture
};
    
} // namespace ns3
#endif