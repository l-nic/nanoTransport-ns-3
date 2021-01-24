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
#include <functional>
#include <bitset>

#include "ns3/object.h"
#include "ns3/traced-callback.h"
#include "ns3/node.h"
#include "ns3/ipv4-header.h"
#include "ns3/nanopu-app-header.h"

// Define module delay in nano seconds
#define REASSEMBLE_DELAY 2
#define PACKETIZATION_DELAY 1

#define NANOPU_APP_HEADER_TYPE 0x9999

namespace ns3 {
    
typedef std::bitset<10240> bitmap_t ;
#define BITMAP_SIZE sizeof(bitmap_t)*8
// TODO: When most messages are small, a bitmap of 
//       this size consumes a lot of memory.
    
uint16_t getFirstSetBitPos(bitmap_t n);
    
bitmap_t setBitMapUntil(uint16_t n);
    
typedef struct reassembleMeta_t {
    uint16_t rxMsgId;
    Ipv4Address srcIp;
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t txMsgId;
    uint16_t msgLen;
    uint16_t pktOffset;
}reassembleMeta_t;
typedef struct rxMsgInfoMeta_t {
    uint16_t rxMsgId;
    uint16_t ackNo; //!< Next expected packet
    uint16_t numPkts; //!< Number of packets in the buffer
    bool isNewMsg;
    bool isNewPkt;
    bool success;
}rxMsgInfoMeta_t;
typedef struct egressMeta_t {
    bool isNewMsg;
    bool isData;
    bool isRtx;
    Ipv4Address dstIP;
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t txMsgId;
    uint16_t msgLen;
    uint16_t pktOffset;
    uint16_t pullOffset;
}egressMeta_t;

/******************************************************************************/

/* Forward declarations for setting the architecture up */
class NanoPuArcht; 
class NanoPuArchtIngressTimer;
class NanoPuArchtEgressTimer; 
    
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
  
  virtual void EgressPipe (Ptr<const Packet> p, egressMeta_t meta);
  
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
  
  void Receive(Ptr<Packet> p, egressMeta_t meta);
  
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

  NanoPuArchtPacketize (Ptr<NanoPuArcht> nanoPuArcht,
                        Ptr<NanoPuArchtArbiter> arbiter);
  ~NanoPuArchtPacketize (void);
  
  void SetTimerModule (Ptr<NanoPuArchtEgressTimer> timer);
  
  void DeliveredEvent (uint16_t txMsgId, uint16_t msgLen,
                       bitmap_t ackPkts);
  
  typedef enum CreditEventOpCode_t
  {
    WRITE = 1,        //!< Write
    ADD  = 2,         //!< Add
    SHIFT_RIGHT  = 4, //!< Shift right
    O1  = 8,          //!< Empty for future reference
    O2  = 16,         //!< Empty for future reference
    O3  = 32,         //!< Empty for future reference
    O4  = 64,         //!< Empty for future reference
    O5  = 128         //!< Empty for future reference
  } CreditEventOpCode_t;
  
  /**
   * \brief The event to update credit of a message.
   *
   * \param txMsgId ID of the message to be processed
   * \param rtxPkt Offset of the packet to be retransmitted (-1 if no RTX is required)
   * \param newCredit Candidate to be the new credit value of the message (-1 if no new credit will be assigned)
   * \param compVal The value to compare to the current credit value
   * \param opCode The code of operation to be applied
   * \param relOp the comparison function to compare compVal and the current credit value
   */
  void CreditToBtxEvent (uint16_t txMsgId, int rtxPkt, int newCredit, int compVal, 
                         CreditEventOpCode_t opCode, std::function<bool(int,int)> relOp);
   
  /**
   * \brief The event to reschedule packets after a timeout.
   *
   * \param txMsgId ID of the message to be processed
   * \param rtxOffset The timer metadata 
   */
  void TimeoutEvent (uint16_t txMsgId, uint16_t rtxOffset);
                         
  int ProcessNewMessage (Ptr<Packet> msg);
  
  void ClearStateForMsg (uint16_t txMsgId);
 
private:

  void Dequeue (uint16_t txMsgId, bitmap_t txPkts, 
                bool isRtx=false, bool isNewMsg=false);
  
protected:
  
  Ptr<NanoPuArcht> m_nanoPuArcht;
  Ptr<NanoPuArchtArbiter> m_arbiter;
  Ptr<NanoPuArchtEgressTimer> m_timer;
  
  std::list<uint16_t> m_txMsgIdFreeList; //!< List of free TX msg IDs
  std::unordered_map<uint16_t, 
                     NanoPuAppHeader> m_appHeaders; //!< table to store app headers, {tx_msg_id => appHeader}
  std::unordered_map<uint16_t,
                     std::map<uint16_t, 
                              Ptr<Packet>>> m_buffers; //!< message packetization buffers, {tx_msg_id => {pktOffset => Packet}}
  std::unordered_map<uint16_t, 
                       bitmap_t> m_deliveredBitmap; //!< bitmap to determine when all pkts are delivered, {tx_msg_id => bitmap}
  std::unordered_map<uint16_t, uint16_t> m_credits; //!< State to track credit for each msg {tx_msg_id => credit} 
  std::unordered_map<uint16_t, 
                       bitmap_t> m_toBeTxBitmap; //!< bitmap to determine which packet to send, {tx_msg_id => bitmap}
  std::unordered_map<uint16_t, uint16_t> m_maxTxPktOffset; //!< State to track max pktOffset sent so far {tx_msg_id => offset}
  std::unordered_map<uint16_t, uint16_t> m_timeoutCnt; //!< State to track number of timeouts {tx_msg_id => timeout count}
};
    
/******************************************************************************/
    
/**
 * \ingroup nanopu-archt
 *
 * \brief Egress Timer Module for NanoPU Architecture
 *
 */
class NanoPuArchtEgressTimer : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArchtEgressTimer (Ptr<NanoPuArcht> nanoPuArcht, 
                          Ptr<NanoPuArchtPacketize> packetize);
  ~NanoPuArchtEgressTimer (void);
  
  void ScheduleTimerEvent (uint16_t txMsgId, uint16_t rtxOffset);
  
  void CancelTimerEvent (uint16_t txMsgId);
  
  void RescheduleTimerEvent (uint16_t txMsgId, uint16_t rtxOffset);
  
  void InvokeTimeoutEvent (uint16_t txMsgId, uint16_t rtxOffset);
  
protected:
  
  Ptr<NanoPuArcht> m_nanoPuArcht;
  Ptr<NanoPuArchtPacketize> m_packetize;
  
  std::unordered_map<uint16_t,EventId> m_timers; //!< state to keep timer meta, {txMsgId => timerMeta}
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

  NanoPuArchtReassemble (Ptr<NanoPuArcht> nanoPuArcht);
  ~NanoPuArchtReassemble (void);
  
  void SetTimerModule (Ptr<NanoPuArchtIngressTimer> timer);
  
  rxMsgInfoMeta_t GetRxMsgInfo (Ipv4Address srcIp, uint16_t srcPort, uint16_t txMsgId,
                                uint16_t msgLen, uint16_t pktOffset);
                                
  void ProcessNewPacket (Ptr<Packet> pkt, reassembleMeta_t meta);
  
  /**
   * \brief The event to free the buffer after a timeout.
   *
   * \param rxMsgId ID of the message to be processed
   */
  void TimeoutEvent (uint16_t rxMsgId);
  
  void ClearStateForMsg (uint16_t rxMsgId);
  
protected:

  Ptr<NanoPuArcht> m_nanoPuArcht;
  Ptr<NanoPuArchtIngressTimer> m_timer;
  
  std::list<uint16_t> m_rxMsgIdFreeList; //!< List of free RX msg IDs
  std::unordered_map<const rxMsgIdTableKey_t, 
                     uint16_t, 
                     rxMsgIdTable_hash, 
                     rxMsgIdTable_key_equal> m_rxMsgIdTable; //!< table that maps {src_ip, src_port, tx_msg_id => rx_msg_id}
  std::unordered_map<uint16_t,
                     std::map<uint16_t, 
                              Ptr<Packet>>> m_buffers; //!< message reassembly buffers, {rx_msg_id => {pktOffset => Packet}}
  std::unordered_map<uint16_t, 
                     bitmap_t> m_receivedBitmap; //!< bitmap to determine when all pkts have arrived, {rx_msg_id => bitmap}
};
    
/******************************************************************************/

/**
 * \ingroup nanopu-archt
 *
 * \brief Ingress Timer Module for NanoPU Architecture
 *
 */
class NanoPuArchtIngressTimer : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArchtIngressTimer (Ptr<NanoPuArcht> nanoPuArcht,
                           Ptr<NanoPuArchtReassemble> reassemble);
  ~NanoPuArchtIngressTimer (void);
  
  void ScheduleTimerEvent (uint16_t rxMsgId);
  
  void CancelTimerEvent (uint16_t rxMsgId);
  
  void InvokeTimeoutEvent (uint16_t rxMsgId);
  
protected:
  
  Ptr<NanoPuArcht> m_nanoPuArcht;
  Ptr<NanoPuArchtReassemble> m_reassemble;
  
  std::unordered_map<uint16_t,EventId> m_timers; //!< state to keep timer meta, {txMsgId => timerMeta}
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

  NanoPuArcht (void);
  ~NanoPuArcht (void);
  
  virtual void AggregateIntoDevice (Ptr<NetDevice> device);
  
  /**
   * \brief Bind the architecture to the device.
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
   * \returns nothing
   */
  virtual void BindToNetDevice ();
  
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
  
  /**
   * \brief Return the node this architecture is associated with.
   * \returns the node
   */
  Ptr<Node> GetNode (void);
  
  /**
   * \brief Returns architecture's Reassembly Buffer.
   *
   * This method allows applications to get the reassembly buffer
   * and set callback to be notified for every reassembled message
   * 
   * \returns Pointer to the reassembly buffer.
   */
  Ptr<NanoPuArchtReassemble> GetReassemblyBuffer (void);
  
  /**
   * \brief Returns architecture's Packetization Buffer.
   * 
   * \returns Pointer to the packetization buffer.
   */
  Ptr<NanoPuArchtPacketize> GetPacketizationBuffer (void);
  
  /**
   * \brief Returns architecture's Arbiter.
   * 
   * \returns Pointer to the arbiter.
   */
  Ptr<NanoPuArchtArbiter> GetArbiter (void);
  
  /**
   * \brief Returns architecture's local IPv4 address.
   * 
   * \returns the local IPv4 Address.
   */
  Ipv4Address GetLocalIp (void);
  
  /**
   * \brief Return the MTU size without the header sizes
   * \returns the payloadSize
   */
  uint16_t GetPayloadSize (void);
  
  /**
   * \brief Return the initial credit given to messages in the beginning
   * \returns the initialCredit
   */
  uint16_t GetInitialCredit (void);
  
  /**
   * \brief Return the maximum number of timeouts allowed before discarding the msg
   * \returns the maxTimeoutCnt
   */
  uint16_t GetMaxTimeoutCnt (void);
  
  /**
   * \brief Return the time to expire a timer
   * \returns the timeoutInterval
   */
  Time GetTimeoutInterval (void);
  
  /**
   * \brief Return maximum number of messages NanoPU can handle at a time
   * \returns the maxNMessages
   */
  uint16_t GetMaxNMessages (void);

  /**
   * \brief Allows applications to set a callback for every reassembled msg on RX
   * \param reassembledMsgCb Callback provided by application
   */
  void SetRecvCallback (Callback<void, Ptr<Packet> > reassembledMsgCb);

  virtual bool EnterIngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                            uint16_t protocol, const Address &from);
                            
  virtual bool SendToNetwork (Ptr<Packet> p);
  
  /**
   * \brief The API for applications to send their messages
   * 
   * \returns whether the msg is accepted by the architecture.
   */
  virtual bool Send (Ptr<Packet> msg);
  
  /**
   * \brief Notifies the application every time a message is reassembled
   * \param msg The reassembled msg that should be given to the application with the app header
   * \param txMsgId The id of the message that was assigned by the sender
   */
  void NotifyApplications (Ptr<Packet> msg, int txMsgId);
  
protected:

  Ptr<NetDevice> m_boundnetdevice; //!< the device this architecture is bound to (might be null).
  Ipv4Address    m_localIp; //!< the local IPv4 Address
    
  uint16_t m_payloadSize;  //!< MTU for the network interface excluding the header sizes
  uint16_t m_initialCredit; //!< Initial window of packets to be sent
  uint16_t m_maxTimeoutCnt; //!< Max allowed number of retransmissions before discarding a msg
  Time m_timeoutInterval;  //!< Time value to expire the timers
  uint16_t m_maxNMessages; //!< Maximum number of messages NanoPU can handle at a time
    
  Ptr<NanoPuArchtReassemble> m_reassemble; //!< the reassembly buffer of the architecture
  Ptr<NanoPuArchtArbiter> m_arbiter; //!< the arbiter of the architecture
  Ptr<NanoPuArchtPacketize> m_packetize; //!< the packetization block of the architecture
  Ptr<NanoPuArchtEgressTimer> m_egressTimer; //!< the egress timer module of the architecture
  Ptr<NanoPuArchtIngressTimer> m_ingressTimer; //!< the ingress timer module of the architecture
    
  Callback<void, Ptr<Packet> > m_reassembledMsgCb; //!< callback to be invoked when a msg is ready to be handed to the application
  
  TracedCallback<Ptr<const Packet>, Ipv4Address, Ipv4Address, uint16_t, uint16_t, int> m_msgBeginTrace;
  TracedCallback<Ptr<const Packet>, Ipv4Address, Ipv4Address, uint16_t, uint16_t, int> m_msgFinishTrace;
};
    
} // namespace ns3
#endif