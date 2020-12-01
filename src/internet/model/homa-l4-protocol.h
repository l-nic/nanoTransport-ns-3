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
#include <map>
#include <functional>
#include <queue>
#include <vector>

#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ip-l4-protocol.h"

namespace ns3 {

class Node;
class Socket;
class Ipv4EndPointDemux;
class Ipv4EndPoint;
class HomaSocket;
class NetDevice;
class HomaSendController;
    
/**
 * \ingroup internet
 * \defgroup homa HOMA
 *
 * This  is  an  implementation of the Homa Transport Protocol described [1].
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
 * maintained as the official ns-3 implementation of the protocol.
 */
    
/**
 * \ingroup homa
 * \brief Implementation of the Homa protocol
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
   * Set node associated with this stack
   * \param node the node
   */
  void SetNode (Ptr<Node> node);
  Ptr<Node> GetNode(void) const;

  virtual int GetProtocolNumber (void) const;
    
  /**
   * \return A smart Socket pointer to a HomaSocket, allocated by this instance
   * of the HOMA protocol
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
   * \brief Send a packet via Homa (IPv4)
   * \param packet The packet to send
   * \param saddr The source Ipv4Address
   * \param daddr The destination Ipv4Address
   * \param sport The source port number
   * \param dport The destination port number
   */
  void Send (Ptr<Packet> packet,
             Ipv4Address saddr, Ipv4Address daddr, 
             uint16_t sport, uint16_t dport);
  /**
   * \brief Send a packet via Homa (IPv4)
   * \param packet The packet to send
   * \param saddr The source Ipv4Address
   * \param daddr The destination Ipv4Address
   * \param sport The source port number
   * \param dport The destination port number
   * \param route The route
   */
  void Send (Ptr<Packet> packet,
             Ipv4Address saddr, Ipv4Address daddr, 
             uint16_t sport, uint16_t dport, Ptr<Ipv4Route> route);
    
  // inherited from Ipv4L4Protocol
  virtual enum IpL4Protocol::RxStatus Receive (Ptr<Packet> p,
                                               Ipv4Header const &header,
                                               Ptr<Ipv4Interface> interface);
  virtual enum IpL4Protocol::RxStatus Receive (Ptr<Packet> p,
                                               Ipv6Header const &header,
                                               Ptr<Ipv6Interface> interface);
    
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
   * This function will notify other components connected to the node that a new stack member is now connected
   * This will be used to notify Layer 3 protocol of layer 4 protocol stack to connect them together.
   */
  virtual void NotifyNewAggregate ();
    
private:
  Ptr<Node> m_node; //!< the node this stack is associated with
  Ipv4EndPointDemux *m_endPoints; //!< A list of IPv4 end points.
    
  std::vector<Ptr<HomaSocket> > m_sockets;      //!< list of sockets
  IpL4Protocol::DownTargetCallback m_downTarget;   //!< Callback to send packets over IPv4
  IpL4Protocol::DownTargetCallback6 m_downTarget6; //!< Callback to send packets over IPv6 (Not supported)
    
  Ptr<HomaSendController> m_sendController;  //!< The controller that manages transmission of HomaOutboundMsg
};
    
/******************************************************************************/
    
/**
 * \ingroup homa
 *
 * \brief Stores the state for outbound Homa messages
 *
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
                   uint32_t mtu, Ptr<Ipv4Route> route=0);
  ~HomaOutboundMsg (void);
  
  uint32_t GetRemainingBytes(void);
  
private:
  std::map<uint16_t, Ptr<Packet>> m_packets; //!< Packet buffer for the message
  Ipv4Address m_saddr; //!< Source IP address of this message
  Ipv4Address m_daddr; //!< Destination IP address of this message
  uint16_t m_sport; //!< Source port of this message
  uint16_t m_dport; //!< Destination port of this message
  Ptr<Ipv4Route> m_route; //!< Route of the message determined by the sender socket 
  
  uint32_t m_maxPayloadSize; 
  uint32_t m_remainingBytes; //!< Remaining number of bytes that are not delivered yet
  uint32_t m_msgSizeBytes;
  uint16_t m_msgSizePkts;
  
};
 
/******************************************************************************/
    
/**
 * \ingroup homa
 *
 * \brief Manages the transmission of all HomaOutboundMsg from HomaL4Protocol
 *
 * This class keeps the state necessary for transmisssion of the messages. 
 * For every new message that arrives from the applications, this class is 
 * responsible for sending the data packet as grants are received.
 *
 */
class HomaSendController : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  static const uint8_t MAX_N_MSG; //!< Maximum number of messages a HomaSendController can hold

  HomaSendController (Ptr<HomaL4Protocol> homaL4Protocol);
  ~HomaSendController (void);
  
  void SetPacer(void);
  
  struct OutboundMsgCompare
  {
    bool operator()(const Ptr<HomaOutboundMsg> left, 
                    const Ptr<HomaOutboundMsg> right)
    {
        return left->GetRemainingBytes() > right->GetRemainingBytes();
    }
  };
  
private:
  Ptr<HomaL4Protocol> m_homa; //!< the protocol instance itself that sends/receives messages
  
  std::list<uint16_t> m_txMsgIdFreeList; //!< List of free TX msg IDs
  
  std::priority_queue<Ptr<HomaOutboundMsg>, 
                      std::vector<Ptr<HomaOutboundMsg>>, 
                      OutboundMsgCompare> m_outboundMsgs;
  
  Time m_pacerLastTxTime; //!< The last simulation time the packet generator sent out a packet
  Time m_packetTxTime; //!< Time to transmit/receive a full MTU packet to/from the network
};
    
} // namespace ns3

#endif /* HOMA_L4_PROTOCOL_H */