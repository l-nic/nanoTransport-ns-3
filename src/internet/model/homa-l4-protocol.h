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

#ifndef HOMA_L4_PROTOCOL_H
#define HOMA_L4_PROTOCOL_H

#include <stdint.h>
#include <vector>
#include <unordered_map>

#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/data-rate.h"
#include "ip-l4-protocol.h"
#include "ns3/homa-header.h"

namespace ns3 {

class Node;
class Socket;
class Ipv4EndPointDemux;
class Ipv4EndPoint;
class HomaSocket;
class NetDevice;
class HomaSendScheduler;
class HomaOutboundMsg;
    
/**
 * \ingroup internet
 * \defgroup homa HOMA
 *
 * This  is  an  implementation of the Homa Transport Protocol described in [1].
 * It implements a connectionless, reliable, low latency message delivery
 * service. 
 *
 * [1] Behnam Montazeri, Yilong Li, Mohammad Alizadeh, and John Ousterhout. 2018. 
 * Homa: a receiver-driven low-latency transport protocol using network 
 * priorities. In <i>Proceedings of the 2018 Conference of the ACM Special Interest 
 * Group on Data Communication</i> (<i>SIGCOMM '18</i>). Association for Computing 
 * Machinery, New York, NY, USA, 221–235. 
 * DOI:https://doi-org.stanford.idm.oclc.org/10.1145/3230543.3230564
 *
 * This implementation is created in guidance of the protocol creators and 
 * maintained as the official ns-3 implementation of the protocol. The IPv6
 * compatibility of the protocol is left for future work.
 */
    
/**
 * \ingroup homa
 * \brief Implementation of the Homa Transport Protocol
 */
class HomaL4Protocol : public IpL4Protocol {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  static const uint8_t PROT_NUMBER; //!< Protocol number of HOMA to be used in IP packets
    
  HomaL4Protocol ();
  virtual ~HomaL4Protocol ();

  /**
   * Set node associated with this stack.
   * \param node The corresponding node.
   */
  void SetNode (Ptr<Node> node);
  /**
   * \brief Get the node associated with this stack.
   * \return The corresponding node.
   */
  Ptr<Node> GetNode(void) const;
    
  /**
   * \brief Get the protocol number associated with Homa Transport.
   * \return The protocol identifier of Homa used in IP headers.
   */
  virtual int GetProtocolNumber (void) const;
    
  /**
   * \brief Create a HomaSocket and associate it with this Homa Protocol instance.
   * \return A smart Socket pointer to a HomaSocket, allocated by the HOMA Protocol.
   */
  Ptr<Socket> CreateSocket (void);
    
  /**
   * \brief Allocate an IPv4 Endpoint
   * \return the Endpoint
   */
  Ipv4EndPoint *Allocate (void);
  /**
   * \brief Allocate an IPv4 Endpoint
   * \param address address to use
   * \return the Endpoint
   */
  Ipv4EndPoint *Allocate (Ipv4Address address);
  /**
   * \brief Allocate an IPv4 Endpoint
   * \param boundNetDevice Bound NetDevice (if any)
   * \param port port to use
   * \return the Endpoint
   */
  Ipv4EndPoint *Allocate (Ptr<NetDevice> boundNetDevice, uint16_t port);
  /**
   * \brief Allocate an IPv4 Endpoint
   * \param boundNetDevice Bound NetDevice (if any)
   * \param address address to use
   * \param port port to use
   * \return the Endpoint
   */
  Ipv4EndPoint *Allocate (Ptr<NetDevice> boundNetDevice, Ipv4Address address, uint16_t port);
  /**
   * \brief Allocate an IPv4 Endpoint
   * \param boundNetDevice Bound NetDevice (if any)
   * \param localAddress local address to use
   * \param localPort local port to use
   * \param peerAddress remote address to use
   * \param peerPort remote port to use
   * \return the Endpoint
   */
  Ipv4EndPoint *Allocate (Ptr<NetDevice> boundNetDevice,
                          Ipv4Address localAddress, uint16_t localPort,
                          Ipv4Address peerAddress, uint16_t peerPort);
    
  /**
   * \brief Remove an IPv4 Endpoint.
   * \param endPoint the end point to remove
   */
  void DeAllocate (Ipv4EndPoint *endPoint);
    
  // called by HomaSocket.
  /**
   * \brief Send a message via Homa Transport Protocol (IPv4)
   * \param message The message to send
   * \param saddr The source Ipv4Address
   * \param daddr The destination Ipv4Address
   * \param sport The source port number
   * \param dport The destination port number
   */
  void Send (Ptr<Packet> message,
             Ipv4Address saddr, Ipv4Address daddr, 
             uint16_t sport, uint16_t dport);
  /**
   * \brief Send a message via Homa Transport Protocol (IPv4)
   * \param message The message to send
   * \param saddr The source Ipv4Address
   * \param daddr The destination Ipv4Address
   * \param sport The source port number
   * \param dport The destination port number
   * \param route The route requested by the sender
   */
  void Send (Ptr<Packet> message,
             Ipv4Address saddr, Ipv4Address daddr, 
             uint16_t sport, uint16_t dport, Ptr<Ipv4Route> route);
  
  // called by HomaSendScheduler.
  /**
   * \brief Send the selected packet down to the IP Layer
   * \param packet The packet to send
   * \param saddr The source Ipv4Address
   * \param daddr The destination Ipv4Address
   * \param route The route requested by the sender
   */ 
  void SendDown (Ptr<Packet> packet, 
                 Ipv4Address saddr, Ipv4Address daddr, 
                 Ptr<Ipv4Route> route);
    
  // inherited from Ipv4L4Protocol
  /**
   * \brief Receive a packet from the lower IP layer
   * \param p The arriving packet from the network
   * \param header The IPv4 header of the arriving packet
   * \param interface The interface from which the packet arrives
   */ 
  virtual enum IpL4Protocol::RxStatus Receive (Ptr<Packet> p,
                                               Ipv4Header const &header,
                                               Ptr<Ipv4Interface> interface);
  virtual enum IpL4Protocol::RxStatus Receive (Ptr<Packet> p,
                                               Ipv6Header const &header,
                                               Ptr<Ipv6Interface> interface);
  
  // inherited from Ipv4L4Protocol (Not used for Homa Transport Purposes)
  virtual void ReceiveIcmp (Ipv4Address icmpSource, uint8_t icmpTtl,
                            uint8_t icmpType, uint8_t icmpCode, uint32_t icmpInfo,
                            Ipv4Address payloadSource,Ipv4Address payloadDestination,
                            const uint8_t payload[8]);
    
  // From IpL4Protocol
  virtual void SetDownTarget (IpL4Protocol::DownTargetCallback cb);
  virtual void SetDownTarget6 (IpL4Protocol::DownTargetCallback6 cb);
    
  // From IpL4Protocol
  virtual IpL4Protocol::DownTargetCallback GetDownTarget (void) const;
  virtual IpL4Protocol::DownTargetCallback6 GetDownTarget6 (void) const;

protected:
  virtual void DoDispose (void);
  /*
   * This function will notify other components connected to the node that a 
   * new stack member is now connected. This will be used to notify Layer 3 
   * protocol of layer 4 protocol stack to connect them together.
   */
  virtual void NotifyNewAggregate ();
    
private:
  Ptr<Node> m_node; //!< the node this stack is associated with
  Ipv4EndPointDemux *m_endPoints; //!< A list of IPv4 end points.
    
  std::vector<Ptr<HomaSocket> > m_sockets;      //!< list of sockets
  IpL4Protocol::DownTargetCallback m_downTarget;   //!< Callback to send packets over IPv4
  IpL4Protocol::DownTargetCallback6 m_downTarget6; //!< Callback to send packets over IPv6 (Not supported)
    
  uint16_t m_bdp; //!< The number of packets required for full utilization, ie. BDP.
  Ptr<HomaSendScheduler> m_sendScheduler;  //!< The scheduler that manages transmission of HomaOutboundMsg
};
    
/******************************************************************************/
    
/**
 * \ingroup homa
 * \brief Stores the state for an outbound Homa message
 */
class HomaOutboundMsg : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  HomaOutboundMsg (Ptr<Packet> message, 
                   Ipv4Address saddr, Ipv4Address daddr, 
                   uint16_t sport, uint16_t dport, 
                   uint32_t mtuBytes, uint16_t rttPackets);
  ~HomaOutboundMsg (void);
  
  /**
   * \brief Set the route requested for this message. (0 if not source-routed)
   * \param route The corresponding route.
   */
  void SetRoute(Ptr<Ipv4Route> route);
  /**
   * \brief Get the route requested for this message. (0 if not source-routed)
   * \return The corresponding route.
   */
  Ptr<Ipv4Route> GetRoute (void);
  
  /**
   * \brief Get the remaining undelivered bytes of this message.
   * \return The amount of undelivered bytes
   */
  uint32_t GetRemainingBytes(void);
  /**
   * \brief Get the total number of packets for this message.
   * \return The number of packets
   */
  uint16_t GetMsgSizePkts(void);
  /**
   * \brief Get the sender's IP address for this message.
   * \return The IPv4 address of the sender
   */
  Ipv4Address GetSrcAddress (void);
  /**
   * \brief Get the receiver's IP address for this message.
   * \return The IPv4 address of the receiver
   */
  Ipv4Address GetDstAddress (void);
  /**
   * \brief Get the sender's port number for this message.
   * \return The port number of the sender
   */
  uint16_t GetSrcPort (void);
  /**
   * \brief Get the receiver's port number for this message.
   * \return The port number of the receiver
   */
  uint16_t GetDstPort (void);
  /**
   * \brief Set the the priority requested for this message by the receiver.
   * \param prio the priority of this message
   */
  void SetPrio(uint8_t prio);
  /**
   * \brief Get the priority requested for this message by the receiver.
   * \param The offset of the packet that priority is being calculated for
   * \return The priority of this message
   */
  uint8_t GetPrio (uint16_t pktOffset);
  
  /**
   * \brief Determines which packet should be sent next for this message
   * \param pktOffset The index of the selected packet (determined inside this function)
   * \param p The selected packet (determined inside this function)
   * \return Whether a packet was successfully selected for this message 
   */
  bool GetNextPacket (uint16_t &pktOffset, Ptr<Packet> &p);
  
  /**
   * \brief Set the the corresponding m_toBeTxPackets entry to false to mark acknowledgement.
   * \param pktOffset The offset of the packet within the message that is to be marked.
   */
  void SetDelivered (uint16_t pktOffset);
  
  /**
   * \brief Set the the corresponding m_deliveredPackets entry to true to mark on-flight or delivered.
   * \param pktOffset The offset of the packet within the message that is to be marked.
   */
  void SetNotToBeTx (uint16_t pktOffset);
  
  /**
   * \brief Update the state per the received Grant
   * \param homaHeader The header information for the received Grant
   */
  void HandleGrant (HomaHeader const &homaHeader);
  
private:
  Ipv4Address m_saddr;       //!< Source IP address of this message
  Ipv4Address m_daddr;       //!< Destination IP address of this message
  uint16_t m_sport;          //!< Source port of this message
  uint16_t m_dport;          //!< Destination port of this message
  Ptr<Ipv4Route> m_route;    //!< Route of the message determined by the sender socket 
  
  std::vector<Ptr<Packet>> m_packets;   //!< Packet buffer for the message
  std::vector<bool> m_deliveredPackets; //!< State to store whether the packets are delivered to the receiver
  std::vector<bool> m_toBeTxPackets;    //!< State to store whether a packet is allowed to be transmitted, ie. not in flight
  
  uint32_t m_maxPayloadSize; //!< Number of bytes that can be stored in packet excluding headers 
  uint32_t m_remainingBytes; //!< Remaining number of bytes that are not delivered yet
  uint32_t m_msgSizeBytes;   //!< Number of bytes this message occupies
  uint16_t m_msgSizePkts;    //!< Number packets this message occupies
  uint16_t m_rttPackets;     //!< Number of packets that is assumed to fit exactly in 1 BDP
  uint16_t m_maxGrantedIdx;  //!< Highest Grant Offset received so far (default: m_rttPackets)
  
  uint8_t m_prio;            //!< The most recent priority of the message
  bool m_prioSetByReceiver;  //!< Whether the receiver has specified a priority yet
};
 
/******************************************************************************/
    
/**
 * \ingroup homa
 *
 * \brief Manages the transmission of all HomaOutboundMsg from HomaL4Protocol
 *
 * This class keeps the state necessary for transmisssion of the messages. 
 * For every new message that arrives from the applications, this class is 
 * responsible for sending the data packets as grants are received.
 *
 */
class HomaSendScheduler : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  static const uint8_t MAX_N_MSG; //!< Maximum number of messages a HomaSendScheduler can hold

  HomaSendScheduler (Ptr<HomaL4Protocol> homaL4Protocol);
  ~HomaSendScheduler (void);
  
  /**
   * \brief Set state values that are used by the tx pacing logic
   */
  void SetPacer (void);
  
  /**
   * \brief Accept a new message from the upper layers and add to the list of pending messages
   * \param outMsg The outbound message to be accepted
   * \return Whether the message was accepted or not
   */
  bool ScheduleNewMessage (Ptr<HomaOutboundMsg> outMsg);
  
  /**
   * \brief Determines which message should be selected to send a packet from
   * \param txMsgId The TX msg ID of the selected message (determined inside this function)
   * \param p The selected packet from the corresponding message (determined inside this function)
   * \return Whether a message was successfully selected
   */
  bool GetNextMsgIdAndPacket (uint16_t &txMsgId, Ptr<Packet> &p);
  
  /**
   * \brief Send the next packet down to the IP layer and schedule next TX.
   */
  void TxPacket(void);
  
  /**
   * \brief Updates the state for the corresponding outbound message per the received GRANT.
   * \param ipv4Header The Ipv4 header of the received GRANT.
   * \param homaHeader The Homa header of the received GRANT.
   */
  void GrantReceivedForOutboundMsg(Ipv4Header const &ipv4Header, 
                                   HomaHeader const &homaHeader);
                           
  /**
   * \brief Updates the state for the corresponding outbound message per the received BUSY.
   * \param ipv4Header The Ipv4 header of the received BUSY.
   * \param homaHeader The Homa header of the received BUSY.
   */
  void BusyReceivedForMsg(Ipv4Header const &ipv4Header, 
                          HomaHeader const &homaHeader);
   
  /**
   * \brief Add the address of the receiver to the list of busy receivers
   * \param receiverAddress The Ipv4 address of the receiver.
   */
  void SetReceiverBusy(Ipv4Address receiverAddress);
  
  /**
   * \brief Remove the address of the receiver from the list of busy receivers
   * \param receiverAddress The Ipv4 address of the receiver.
   */
  void SetReceiverNotBusy(Ipv4Address receiverAddress);
  
private:
  Ptr<HomaL4Protocol> m_homa; //!< the protocol instance itself that sends/receives messages
  Time m_pacerLastTxTime;     //!< The last simulation time the packet generator sent out a packet
  DataRate m_txRate;          //!< Data Rate of the corresponding net device for this prototocol
  EventId m_txEvent;          //!< The EventID for the next scheduled transmission
  
  std::list<uint16_t> m_txMsgIdFreeList;  //!< List of free TX msg IDs
  std::unordered_map<uint16_t, Ptr<HomaOutboundMsg>> m_outboundMsgs; //!< state to keep HomaOutboundMsg with the key as txMsgId
  std::list<Ipv4Address> m_busyReceivers; //!< List of busy receivers that packets shouldn't be sent to
};
    
} // namespace ns3

#endif /* HOMA_L4_PROTOCOL_H */