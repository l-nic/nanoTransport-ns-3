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

#include <algorithm>

#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/boolean.h"
#include "ns3/object-vector.h"
#include "ns3/uinteger.h"

#include "ns3/point-to-point-net-device.h"
#include "ns3/ppp-header.h"
#include "ns3/ipv4-route.h"
#include "ipv4-end-point-demux.h"
#include "ipv4-end-point.h"
#include "ipv4-l3-protocol.h"
#include "homa-l4-protocol.h"
#include "homa-socket-factory.h"
#include "homa-socket.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HomaL4Protocol");

NS_OBJECT_ENSURE_REGISTERED (HomaL4Protocol);

/* The protocol is not standardized yet. Using a temporary number */
const uint8_t HomaL4Protocol::PROT_NUMBER = 198;
    
TypeId 
HomaL4Protocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaL4Protocol")
    .SetParent<IpL4Protocol> ()
    .SetGroupName ("Internet")
    .AddConstructor<HomaL4Protocol> ()
    .AddAttribute ("SocketList", "The list of sockets associated to this protocol.",
                   ObjectVectorValue (),
                   MakeObjectVectorAccessor (&HomaL4Protocol::m_sockets),
                   MakeObjectVectorChecker<HomaSocket> ())
    .AddAttribute ("RttPackets", "The number of packets required for full utilization, ie. BDP.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&HomaL4Protocol::m_bdp),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}
    
HomaL4Protocol::HomaL4Protocol ()
  : m_endPoints (new Ipv4EndPointDemux ())
{
  NS_LOG_FUNCTION (this);
      
  m_sendScheduler = CreateObject<HomaSendScheduler> (this);
  m_recvScheduler = CreateObject<HomaRecvScheduler> (this);
}

HomaL4Protocol::~HomaL4Protocol ()
{
  NS_LOG_FUNCTION_NOARGS ();
}
    
void 
HomaL4Protocol::SetNode (Ptr<Node> node)
{
  m_node = node;
  m_mtu = m_node->GetDevice (0)->GetMtu ();
}
    
Ptr<Node> 
HomaL4Protocol::GetNode(void) const
{
  return m_node;
}
    
int 
HomaL4Protocol::GetProtocolNumber (void) const
{
  return PROT_NUMBER;
}
    
/*
 * This method is called by AggregateObject and completes the aggregation
 * by setting the node in the homa stack and link it to the ipv4 object
 * present in the node along with the socket factory
 */
void
HomaL4Protocol::NotifyNewAggregate ()
{
  NS_LOG_FUNCTION (this);
  Ptr<Node> node = this->GetObject<Node> ();
  Ptr<Ipv4> ipv4 = this->GetObject<Ipv4> ();
    
  NS_ASSERT_MSG(ipv4, "Homa L4 Protocol supports only IPv4.");

  if (m_node == 0)
    {
      if ((node != 0) && (ipv4 != 0))
        {
          this->SetNode (node);
          Ptr<HomaSocketFactory> homaFactory = CreateObject<HomaSocketFactory> ();
          homaFactory->SetHoma (this);
          node->AggregateObject (homaFactory);
          
          m_sendScheduler->SetPacer();
          
          NS_ASSERT(m_mtu); // m_mtu is set inside SetNode() above.
          m_recvScheduler->SetMtuAndBdp (m_mtu, m_bdp);
        }
    }
  
  if (ipv4 != 0 && m_downTarget.IsNull())
    {
      // We register this HomaL4Protocol instance as one of the upper targets of the IP layer
      ipv4->Insert (this);
      // We set our down target to the IPv4 send function.
      this->SetDownTarget (MakeCallback (&Ipv4::Send, ipv4));
    }
  IpL4Protocol::NotifyNewAggregate ();
}
    
void
HomaL4Protocol::DoDispose (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  for (std::vector<Ptr<HomaSocket> >::iterator i = m_sockets.begin (); i != m_sockets.end (); i++)
    {
      *i = 0;
    }
  m_sockets.clear ();

  if (m_endPoints != 0)
    {
      delete m_endPoints;
      m_endPoints = 0;
    }
  
  m_node = 0;
  m_downTarget.Nullify ();
/*
 = MakeNullCallback<void,Ptr<Packet>, Ipv4Address, Ipv4Address, uint8_t, Ptr<Ipv4Route> > ();
*/
  IpL4Protocol::DoDispose ();
}
 
/*
 * This method is called by HomaSocketFactory associated with m_node which 
 * returns a socket that is tied to this HomaL4Protocol instance.
 */
Ptr<Socket>
HomaL4Protocol::CreateSocket (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Ptr<HomaSocket> socket = CreateObject<HomaSocket> ();
  socket->SetNode (m_node);
  socket->SetHoma (this);
  m_sockets.push_back (socket);
  return socket;
}
    
Ipv4EndPoint *
HomaL4Protocol::Allocate (void)
{
  NS_LOG_FUNCTION (this);
  return m_endPoints->Allocate ();
}

Ipv4EndPoint *
HomaL4Protocol::Allocate (Ipv4Address address)
{
  NS_LOG_FUNCTION (this << address);
  return m_endPoints->Allocate (address);
}

Ipv4EndPoint *
HomaL4Protocol::Allocate (Ptr<NetDevice> boundNetDevice, uint16_t port)
{
  NS_LOG_FUNCTION (this << boundNetDevice << port);
  return m_endPoints->Allocate (boundNetDevice, port);
}

Ipv4EndPoint *
HomaL4Protocol::Allocate (Ptr<NetDevice> boundNetDevice, Ipv4Address address, uint16_t port)
{
  NS_LOG_FUNCTION (this << boundNetDevice << address << port);
  return m_endPoints->Allocate (boundNetDevice, address, port);
}
Ipv4EndPoint *
HomaL4Protocol::Allocate (Ptr<NetDevice> boundNetDevice,
                         Ipv4Address localAddress, uint16_t localPort,
                         Ipv4Address peerAddress, uint16_t peerPort)
{
  NS_LOG_FUNCTION (this << boundNetDevice << localAddress << localPort << peerAddress << peerPort);
  return m_endPoints->Allocate (boundNetDevice,
                                localAddress, localPort,
                                peerAddress, peerPort);
}

void 
HomaL4Protocol::DeAllocate (Ipv4EndPoint *endPoint)
{
  NS_LOG_FUNCTION (this << endPoint);
  m_endPoints->DeAllocate (endPoint);
}
    
void
HomaL4Protocol::Send (Ptr<Packet> message, 
                     Ipv4Address saddr, Ipv4Address daddr, 
                     uint16_t sport, uint16_t dport)
{
  NS_LOG_FUNCTION (this << message << saddr << daddr << sport << dport);
    
  Send(message, saddr, daddr, sport, dport, 0);
}
    
void
HomaL4Protocol::Send (Ptr<Packet> message, 
                     Ipv4Address saddr, Ipv4Address daddr, 
                     uint16_t sport, uint16_t dport, Ptr<Ipv4Route> route)
{
  NS_LOG_FUNCTION (this << message << saddr << daddr << sport << dport << route);
    
  
  Ptr<HomaOutboundMsg> outMsg = CreateObject<HomaOutboundMsg> (message, saddr, daddr, sport, dport, 
                                                              m_mtu, m_bdp);
  outMsg->SetRoute (route); // This is mostly unnecessary
  m_sendScheduler->ScheduleNewMessage(outMsg);
}

/*
 * This method is called by the associated HomaSendScheduler after the  
 * next data packet to transmit is selected. The selected packet is then
 * pushed down to the lower IP layer.
 */
void
HomaL4Protocol::SendDown (Ptr<Packet> packet, 
                          Ipv4Address saddr, Ipv4Address daddr, 
                          Ptr<Ipv4Route> route)
{
  NS_LOG_FUNCTION (this << packet << saddr << daddr << route);
    
  m_downTarget (packet, saddr, daddr, PROT_NUMBER, route);
}

/*
 * This method is called by the lower IP layer to notify arrival of a 
 * new packet from the network. The method then classifies the packet
 * and forward it to the appropriate scheduler (send or receive) to have
 * Homa Transport logic applied on it.
 */
enum IpL4Protocol::RxStatus
HomaL4Protocol::Receive (Ptr<Packet> packet,
                        Ipv4Header const &header,
                        Ptr<Ipv4Interface> interface)
{
  NS_LOG_FUNCTION (this << packet << header << interface);
    
  NS_LOG_DEBUG ("HomaL4Protocol received: " << packet->ToString ());
    
  NS_ASSERT(header.GetProtocol() == PROT_NUMBER);
  
  Ptr<Packet> cp = packet->Copy ();
    
  HomaHeader homaHeader;
  cp->RemoveHeader(homaHeader);

  NS_LOG_DEBUG ("Looking up dst " << header.GetDestination () << " port " << homaHeader.GetDstPort ()); 
  Ipv4EndPointDemux::EndPoints endPoints =
    m_endPoints->Lookup (header.GetDestination (), homaHeader.GetDstPort (),
                         header.GetSource (), homaHeader.GetSrcPort (), interface);
  if (endPoints.empty ())
    {
      NS_LOG_LOGIC ("RX_ENDPOINT_UNREACH");
      return IpL4Protocol::RX_ENDPOINT_UNREACH;
    }
    
  //  The Homa protocol logic starts here!
  uint8_t rxFlag = homaHeader.GetFlags ();
  if (rxFlag & HomaHeader::Flags_t::DATA)
  {
    m_recvScheduler->ReceiveDataPacket(cp, header, homaHeader, interface);
  }
  else if ((rxFlag & HomaHeader::Flags_t::GRANT) ||
           (rxFlag & HomaHeader::Flags_t::RESEND))
  {
    m_sendScheduler->CtrlPktRecvdForOutboundMsg(header, homaHeader);
  }
  else if (rxFlag & HomaHeader::Flags_t::BUSY)
  {
    m_sendScheduler->BusyReceivedForMsg(header, homaHeader);
    // TODO: The busy packet is always sent from sender to receiver. 
  }
  else
  {
    NS_LOG_ERROR("HomaL4Protocol received an unknown type of a packet: " 
                 << homaHeader.FlagsToString(rxFlag));
    return IpL4Protocol::RX_ENDPOINT_UNREACH;
  }
  return IpL4Protocol::RX_OK;
}
    
enum IpL4Protocol::RxStatus
HomaL4Protocol::Receive (Ptr<Packet> packet,
                        Ipv6Header const &header,
                        Ptr<Ipv6Interface> interface)
{
  NS_FATAL_ERROR_CONT("HomaL4Protocol currently doesn't support IPv6. Use IPv4 instead.");
  return IpL4Protocol::RX_ENDPOINT_UNREACH;
}

/*
 * This method is called by the HomaRecvScheduler everytime a message is ready to
 * be forwarded up to the applications. 
 */
void HomaL4Protocol::ForwardUp (Ptr<Packet> completeMsg,
                                const Ipv4Header &header,
                                uint16_t sport, uint16_t dport,
                                Ptr<Ipv4Interface> incomingInterface)
{
  NS_LOG_FUNCTION (this << completeMsg << header << sport << incomingInterface);
    
  NS_LOG_DEBUG ("Looking up dst " << header.GetDestination () << " port " << dport); 
  Ipv4EndPointDemux::EndPoints endPoints =
    m_endPoints->Lookup (header.GetDestination (), dport,
                         header.GetSource (), sport, incomingInterface);
    
  NS_ASSERT_MSG(!endPoints.empty (), "HomaL4Protocol was able to find an endpoint when msg was received, but now it couldn't");
    
  for (Ipv4EndPointDemux::EndPointsI endPoint = endPoints.begin ();
         endPoint != endPoints.end (); endPoint++)
  {
    (*endPoint)->ForwardUp (completeMsg, header, sport, incomingInterface);
  }
}

// inherited from Ipv4L4Protocol (Not used for Homa Transport Purposes)
void 
HomaL4Protocol::ReceiveIcmp (Ipv4Address icmpSource, uint8_t icmpTtl,
                            uint8_t icmpType, uint8_t icmpCode, uint32_t icmpInfo,
                            Ipv4Address payloadSource,Ipv4Address payloadDestination,
                            const uint8_t payload[8])
{
  NS_LOG_FUNCTION (this << icmpSource << icmpTtl << icmpType << icmpCode << icmpInfo 
                        << payloadSource << payloadDestination);
  uint16_t src, dst;
  src = payload[0] << 8;
  src |= payload[1];
  dst = payload[2] << 8;
  dst |= payload[3];

  Ipv4EndPoint *endPoint = m_endPoints->SimpleLookup (payloadSource, src, payloadDestination, dst);
  if (endPoint != 0)
    {
      endPoint->ForwardIcmp (icmpSource, icmpTtl, icmpType, icmpCode, icmpInfo);
    }
  else
    {
      NS_LOG_DEBUG ("no endpoint found source=" << payloadSource <<
                    ", destination="<<payloadDestination<<
                    ", src=" << src << ", dst=" << dst);
    }
}
    
void
HomaL4Protocol::SetDownTarget (IpL4Protocol::DownTargetCallback callback)
{
  NS_LOG_FUNCTION (this);
  m_downTarget = callback;
}
    
void
HomaL4Protocol::SetDownTarget6 (IpL4Protocol::DownTargetCallback6 callback)
{
  NS_LOG_FUNCTION (this);
  NS_FATAL_ERROR("HomaL4Protocol currently doesn't support IPv6. Use IPv4 instead.");
  m_downTarget6 = callback;
}

IpL4Protocol::DownTargetCallback
HomaL4Protocol::GetDownTarget (void) const
{
  return m_downTarget;
}
    
IpL4Protocol::DownTargetCallback6
HomaL4Protocol::GetDownTarget6 (void) const
{
  NS_FATAL_ERROR("HomaL4Protocol currently doesn't support IPv6. Use IPv4 instead.");
  return m_downTarget6;
}

/******************************************************************************/

TypeId HomaOutboundMsg::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaOutboundMsg")
    .SetParent<Object> ()
    .SetGroupName("Internet")
  ;
  return tid;
}

/*
 * This method creates a new outbound message with the given information.
 */
HomaOutboundMsg::HomaOutboundMsg (Ptr<Packet> message, 
                                  Ipv4Address saddr, Ipv4Address daddr, 
                                  uint16_t sport, uint16_t dport, 
                                  uint32_t mtuBytes, uint16_t rttPackets)
    : m_route(0),
      m_prio(0),
      m_prioSetByReceiver(false)
{
  NS_LOG_FUNCTION (this);
      
  m_saddr = saddr;
  m_daddr = daddr;
  m_sport = sport;
  m_dport = dport;
    
  m_msgSizeBytes = message->GetSize ();
  // The remaining undelivered message size equals to the total message size in the beginning
  m_remainingBytes = m_msgSizeBytes;
    
  HomaHeader homah;
  Ipv4Header ipv4h;
  m_maxPayloadSize = mtuBytes - homah.GetSerializedSize () - ipv4h.GetSerializedSize ();
    
  // Packetize the message into MTU sized packets and store the corresponding state
  uint32_t unpacketizedBytes = m_msgSizeBytes;
  uint16_t numPkts = 0;
  uint32_t nextPktSize;
  Ptr<Packet> nextPkt;
  while (unpacketizedBytes > 0)
  {
    nextPktSize = std::min(unpacketizedBytes, m_maxPayloadSize);
    nextPkt = message->CreateFragment (message->GetSize () - unpacketizedBytes, nextPktSize);

    m_packets.push_back(nextPkt);
    m_deliveredPackets.push_back(false);
    m_toBeTxPackets.push_back(true);

    unpacketizedBytes -= nextPktSize;
    numPkts ++;
  } 
  m_msgSizePkts = numPkts;
          
  m_rttPackets = rttPackets;
  m_maxGrantedIdx = m_rttPackets;
}

HomaOutboundMsg::~HomaOutboundMsg ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void HomaOutboundMsg::SetRoute(Ptr<Ipv4Route> route)
{
  m_route = route;
}
    
Ptr<Ipv4Route> HomaOutboundMsg::GetRoute ()
{
  return m_route;
}
    
uint32_t HomaOutboundMsg::GetRemainingBytes()
{
  return m_remainingBytes;
}
    
uint16_t HomaOutboundMsg::GetMsgSizePkts()
{
  return m_msgSizePkts;
}
    
Ipv4Address HomaOutboundMsg::GetSrcAddress ()
{
  return m_saddr;
}
    
Ipv4Address HomaOutboundMsg::GetDstAddress ()
{
  return m_daddr;
}

uint16_t HomaOutboundMsg::GetSrcPort ()
{
  return m_sport;
}
    
uint16_t HomaOutboundMsg::GetDstPort ()
{
  return m_dport;
}
    
void HomaOutboundMsg::SetPrio (uint8_t prio)
{
  m_prio = prio;
}
    
uint8_t HomaOutboundMsg::GetPrio (uint16_t pktOffset)
{
  if (!m_prioSetByReceiver)
  {
    // Determine the priority of the unscheduled packet
    
    // TODO: Determine priority of unscheduled packet (index = pktOffset)
    //       according to the distribution of message sizes.
    return 0;
  }
  return m_prio;
}
    
bool HomaOutboundMsg::GetNextPacket (uint16_t &pktOffset, Ptr<Packet> &p)
{
  NS_LOG_FUNCTION (this);
    
  uint16_t i = 0;
  while (i <= m_maxGrantedIdx && i <= m_msgSizePkts)
  {
    if (!m_deliveredPackets[i] && m_toBeTxPackets[i])
    {
      // The selected packet is not delivered and not on flight
      NS_LOG_LOGIC("HomaOutboundMsg (" << this 
                   << ") will send packet " << i << " next.");
      pktOffset = i;
      p = m_packets[i];
      return true;
    }
    i++;
  }
  NS_LOG_LOGIC("HomaOutboundMsg (" << this 
               << ") doesn't have any packet to send!");
  return false;
}
    
/*
 * This method is called when a GRANT or a BUSY packet is received. 
 * The pktOffset value on a control packet denotes that the receiver
 * has received the corresponding data packet, so the packet can be 
 * marked delivered.
 */    
void HomaOutboundMsg::SetDelivered (uint16_t pktOffset)
{
  NS_LOG_FUNCTION (this << pktOffset);
    
  NS_LOG_DEBUG("HomaOutboundMsg (" << this 
               << ") is marking packet " << pktOffset 
               << " as delivered.");
    
  m_deliveredPackets[pktOffset] = true;
}

/*
 * This method is called when a packet is transmitted to note that it
 * is no longer allowed to be transmitted again. If a timeout occurs 
 * for this message, the corresponding m_toBeTxPackets entry will be
 * set back to true.
 */
void HomaOutboundMsg::SetNotToBeTx (uint16_t pktOffset)
{
  NS_LOG_FUNCTION (this << pktOffset);
    
  NS_LOG_DEBUG("HomaOutboundMsg (" << this 
               << ") is marking packet " << pktOffset 
               << " as not 'to be transmitted'.");
    
  m_toBeTxPackets[pktOffset] = false;
}
    
/*
 * This method updates the state for the corresponding outbound message
 * upon reveival of a Grant. The state is updated only if the granted 
 * packet index is larger than the highest grant index received so far.
 * This allows reordered Grants to be ignored when more recent ones are
 * received.
 */
void HomaOutboundMsg::HandleGrant (HomaHeader const &homaHeader)
{
  NS_LOG_FUNCTION (this << homaHeader);
    
  // TODO: Figure out what to do when RESEND is received
//   NS_ASSERT(homaHeader.GetFlags() & HomaHeader::Flags_t::GRANT);
    
  uint16_t grantOffset = homaHeader.GetGrantOffset();
    
  if (grantOffset >= m_maxGrantedIdx)
  {
    uint8_t prio = homaHeader.GetPrio();
    NS_LOG_LOGIC("HomaOutboundMsg (" << this 
                 << ") is increasing the Grant index to "
                 << grantOffset << " and setting priority as "
                 << prio);
    m_maxGrantedIdx = grantOffset;
      
    m_prio = prio;
    m_prioSetByReceiver = true;
      
    /*
     * Since Homa doesn't explicitly acknowledge the delivery of data packets,
     * one way to estimate the remaining bytes is to exploit the mechanism where
     * Homa grants messages in a way that there is always exactly BDP worth of 
     * packet on flight. Then we can calculate the remaining bytes as the following.
     */
    m_remainingBytes = (grantOffset - m_rttPackets) * m_maxPayloadSize;
  }
  else
  {
    NS_LOG_LOGIC("HomaOutboundMsg (" << this 
                 << ") has received an out-of-order Grant. State is not updated!");
  }
}
    
/******************************************************************************/
    
const uint16_t HomaSendScheduler::MAX_N_MSG = 255;

TypeId HomaSendScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaSendScheduler")
    .SetParent<Object> ()
    .SetGroupName("Internet")
  ;
  return tid;
}
    
HomaSendScheduler::HomaSendScheduler (Ptr<HomaL4Protocol> homaL4Protocol)
{
  NS_LOG_FUNCTION (this);
      
  m_homa = homaL4Protocol;
  
  // Initially, all the txMsgId values between 0 and MAX_N_MSG are listed as free
  m_txMsgIdFreeList.resize(MAX_N_MSG);
  std::iota(m_txMsgIdFreeList.begin(), m_txMsgIdFreeList.end(), 0);
}

HomaSendScheduler::~HomaSendScheduler ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

/*
 * This method is called by the NotifyNewAggregate() method of the 
 * associated HomaL4Protocol instance to calculate dataRate of the 
 * corresponding NetDevice, so that TX decisions are made after the 
 * previous packet is completely serialized. This allows scheduling 
 * decisions to be made as late as possible which is valuable to make 
 * sure the message with the shortest remaining size most recently is
 * selected.
 */
void HomaSendScheduler::SetPacer ()
{
  NS_LOG_FUNCTION (this);
    
  Ptr<NetDevice> netDevice = m_homa->GetNode ()->GetDevice (0);
  PointToPointNetDevice* p2pNetDevice = dynamic_cast<PointToPointNetDevice*>(&(*(netDevice))); 
    
  m_txRate = p2pNetDevice->GetDataRate ();
  m_pacerLastTxTime = Simulator::Now () - m_txRate.CalculateBytesTxTime ((uint32_t) netDevice->GetMtu ());
}

/*
 * This method is called upon receiving a new message from the application layer.
 * It inserts the message into the list of pending outbound messages and updates
 * the scheduler's state accordingly.
 */
bool 
HomaSendScheduler::ScheduleNewMessage (Ptr<HomaOutboundMsg> outMsg)
{
  NS_LOG_FUNCTION (this << outMsg);
    
  uint16_t txMsgId;
  if (m_txMsgIdFreeList.size() > 0)
  {
    // Assign a txMsgId which will be unique tothis message on this host while the message lasts
    txMsgId = m_txMsgIdFreeList.front ();
    NS_LOG_LOGIC("HomaSendScheduler allocating txMsgId: " << txMsgId);
    m_txMsgIdFreeList.pop_front ();
      
    m_outboundMsgs[txMsgId] = outMsg;
      
    /* 
     * HomaSendScheduler can send a packet if it hasn't done so
     * recently. Otwerwise a txEvent should already been scheduled
     * which iterates over m_outboundMsgs to decide which packet 
     * to send next. 
     */
    if(m_txEvent.IsExpired()) 
      this->TxDataPacket();
  }
  else
  {
    NS_LOG_ERROR(Simulator::Now ().GetNanoSeconds () << 
                 " Error: HomaSendScheduler ("<< this << 
                 ") could not allocate a new txMsgId for message (" << 
                 outMsg << ")" );
    return false;
  }
  return true;
}
   
/*
 * This method determines the txMsgId of the highest priority 
 * message that is ready to send some packets into the network.
 * See the nested if statements for the algorithm to choose 
 * highest priority outbound message.
 */
bool HomaSendScheduler::GetNextMsgId (uint16_t &txMsgId)
{
  NS_LOG_FUNCTION (this);
  
  Ptr<HomaOutboundMsg> currentMsg;
  Ptr<HomaOutboundMsg> candidateMsg;
  uint32_t minRemainingBytes = std::numeric_limits<uint32_t>::max();
  uint16_t pktOffset;
  Ptr<Packet> p;
  bool msgSelected = false;
  /*
   * Iterate over all pending outbound messages and select the one 
   * that has the smallest remainingBytes, granted but not transmitted 
   * packets, and a receiver that is not listed as busy.
   */
  for (auto& it: m_outboundMsgs) 
  {
    currentMsg = it.second;
    uint32_t curRemainingBytes = currentMsg->GetRemainingBytes();
    // Accept current msg if the remainig size is smaller than minRemainingBytes
    if (curRemainingBytes < minRemainingBytes)
    {
      Ipv4Address daddr = currentMsg->GetDstAddress ();
      // Accept current msg if the receiver is not busy
      if (std::find(m_busyReceivers.begin(),m_busyReceivers.end(),daddr) == m_busyReceivers.end())
      {
        // Accept current msg if it has a granted but not transmitted packet
        if (currentMsg->GetNextPacket(pktOffset, p))
        {
          candidateMsg = currentMsg;
          txMsgId = it.first;
          minRemainingBytes = curRemainingBytes;
          msgSelected = true;
        }
      }
    }
  }
  return msgSelected;
}
    
bool HomaSendScheduler::GetNextPktOfMsg (uint16_t txMsgId, Ptr<Packet> &p)
{
  NS_LOG_FUNCTION (this << txMsgId);
  
  uint16_t pktOffset;
  Ptr<HomaOutboundMsg> candidateMsg = m_outboundMsgs[txMsgId];
    
  if (candidateMsg->GetNextPacket(pktOffset, p))
  {
    HomaHeader homaHeader;
    homaHeader.SetDstPort (candidateMsg->GetDstPort ());
    homaHeader.SetSrcPort (candidateMsg->GetSrcPort ());
    homaHeader.SetTxMsgId (txMsgId);
    homaHeader.SetFlags (HomaHeader::Flags_t::DATA); 
    homaHeader.SetMsgLen (candidateMsg->GetMsgSizePkts ());
    homaHeader.SetPktOffset (pktOffset);
    homaHeader.SetPayloadSize (p->GetSize ());
    
    // NOTE: Use the following SocketIpTosTag append strategy when 
    //       sending packets out. This allows us to set the priority
    //       of the packets correctly for the PfifoHomaQueueDisc way of 
    //       priority queueing in the network.
    SocketIpTosTag ipTosTag;
    ipTosTag.SetTos (candidateMsg->GetPrio (pktOffset)); 
    // This packet may already have a SocketIpTosTag (see HomaSocket)
    p->ReplacePacketTag (ipTosTag);
      
    /*
     * The priority of packets are actually carried on the packet tags as
     * shown above. The priority field on the homaHeader field is actually
     * used by control packets to signal the requested priority from receivers
     * to the senders, so that they can set their data packet priorities 
     * accordingly.
     *
     * Setting the priority field on a data packet is just for monitoring reasons.
     */
    homaHeader.SetPrio (candidateMsg->GetPrio (pktOffset));
      
    p->AddHeader (homaHeader);
    NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " HomaL4Protocol sending: " << p->ToString ());
    
    /*
     * Set the corresponding packet as "not to be sent" (ie. on-flight)
     * to prevent redundant re-transmissions.
     */
    candidateMsg->SetNotToBeTx(pktOffset);
    return true;
  }
  else
  {
    return false;
  }
}
 
/*
 * This method is called either when a new packet to send is found after
 * an idle time period or when the serialization of the previous packet finishes.
 * This allows HomaSendScheduler to choose the most recent highest priority packet
 * just before sending it.
 */
void
HomaSendScheduler::TxDataPacket ()
{
  NS_LOG_FUNCTION (this);
    
  uint16_t nextTxMsgID;
  Ptr<Packet> p;
  if (this->GetNextMsgId (nextTxMsgID))
  {
    NS_LOG_LOGIC("HomaSendScheduler will transmit a packet from msg " << nextTxMsgID);
      
    NS_ASSERT(this->GetNextPktOfMsg(nextTxMsgID, p));
      
    Ptr<HomaOutboundMsg> nextMsg = m_outboundMsgs[nextTxMsgID];
    Ipv4Address saddr = nextMsg->GetSrcAddress ();
    Ipv4Address daddr = nextMsg->GetDstAddress ();
    Ptr<Ipv4Route> route = nextMsg->GetRoute ();
    
    m_homa->SendDown(p, saddr, daddr, route);
      
    /* 
     * Calculate the time it would take to serialize this packet and schedule
     * the next TX of this scheduler accordingly.
     */
    Ipv4Header iph;
    PppHeader pph;
    uint32_t headerSize = iph.GetSerializedSize () + pph.GetSerializedSize ();
    Time txTime = m_txRate.CalculateBytesTxTime (p->GetSize () + headerSize);
    m_txEvent = Simulator::Schedule (txTime, &HomaSendScheduler::TxDataPacket, this);
  }
  else
  {
    NS_LOG_LOGIC("HomaSendScheduler doesn't have any packet to send!");
  }
}
   
/*
 * This method is called when a control packet is received that interests
 * an outbound message.
 */
void HomaSendScheduler::CtrlPktRecvdForOutboundMsg(Ipv4Header const &ipv4Header, 
                                                   HomaHeader const &homaHeader)
{
  NS_LOG_FUNCTION (this << ipv4Header << homaHeader);
    
  Ptr<HomaOutboundMsg> targetdMsg = m_outboundMsgs[homaHeader.GetTxMsgId()];
  // Verify that the TxMsgId indeed matches the 4 tuple
  NS_ASSERT(targetdMsg->GetSrcAddress() == ipv4Header.GetSource ());
  NS_ASSERT(targetdMsg->GetDstAddress() == ipv4Header.GetDestination ());
  NS_ASSERT(targetdMsg->GetSrcPort() == homaHeader.GetSrcPort ());
  NS_ASSERT(targetdMsg->GetDstPort() == homaHeader.GetDstPort ());
    
  /*
   * The pktOffset within GRANT and BUSY packets are used to acknowledge 
   * the arrival of the corresponding data packet to the receiver.
   */
  targetdMsg->SetDelivered (homaHeader.GetPktOffset ());
  
  uint8_t ctrlFlag = homaHeader.GetFlags();
  if (ctrlFlag & HomaHeader::Flags_t::GRANT)
  {
    targetdMsg->HandleGrant (homaHeader);
  }
  else if (ctrlFlag & HomaHeader::Flags_t::RESEND)
  {
    // TODO: Figure out what to do when RESEND is received
    targetdMsg->HandleGrant (homaHeader);
  }
  else
  {
    NS_LOG_ERROR("HomaSendScheduler (" << this 
                 << ") has received an unexpected control packet ("
                 << homaHeader.FlagsToString(ctrlFlag) << ")");
  }
  
  // Since the receiver is sending GRANTs, it is considered not busy
  this->SetReceiverNotBusy(ipv4Header.GetDestination ());
  // TODO: If there are multiple messages from the same sender to the same 
  //       receiver, all of those messages will be allowed to send at once.
    
  /* 
   * Since control packets may allow new packets to be sent, we should try 
   * to transmit those packets.
   */
  if(m_txEvent.IsExpired()) 
    this->TxDataPacket();
}
    
void HomaSendScheduler::BusyReceivedForMsg(Ipv4Header const &ipv4Header, 
                                           HomaHeader const &homaHeader)
{
  NS_LOG_FUNCTION (this << ipv4Header << homaHeader);
    
  Ptr<HomaOutboundMsg> busyMsg = m_outboundMsgs[homaHeader.GetTxMsgId()];
  // Verify that the TxMsgId indeed matches the 4 tuple
  NS_ASSERT(busyMsg->GetSrcAddress() == ipv4Header.GetSource ());
  NS_ASSERT(busyMsg->GetDstAddress() == ipv4Header.GetDestination ());
  NS_ASSERT(busyMsg->GetSrcPort() == homaHeader.GetSrcPort ());
  NS_ASSERT(busyMsg->GetDstPort() == homaHeader.GetDstPort ());
    
  /*
   * The pktOffset within GRANT and BUSY packets are used to acknowledge 
   * the arrival of the corresponding data packet to the receiver.
   */
  busyMsg->SetDelivered (homaHeader.GetPktOffset ());
    
  this->SetReceiverBusy(ipv4Header.GetDestination ());
  // TODO: If there are multiple messages from the same sender to the same 
  //       receiver where only one of them is granted by the receiver, the 
  //       busy packet for the other messages will block the granted one as well.
}
    
void HomaSendScheduler::SetReceiverBusy(Ipv4Address receiverAddress)
{
  NS_LOG_FUNCTION (this << receiverAddress);
    
  if (std::find(m_busyReceivers.begin(),m_busyReceivers.end(),receiverAddress) == m_busyReceivers.end())
  {
    m_busyReceivers.push_back(receiverAddress);
  }
}
    
void HomaSendScheduler::SetReceiverNotBusy(Ipv4Address receiverAddress)
{
  NS_LOG_FUNCTION (this << receiverAddress);
    
  m_busyReceivers.remove(receiverAddress);
}
    
/******************************************************************************/

TypeId HomaInboundMsg::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaInboundMsg")
    .SetParent<Object> ()
    .SetGroupName("Internet")
  ;
  return tid;
}

/*
 * This method creates a new inbound message with the given information.
 */
HomaInboundMsg::HomaInboundMsg (Ptr<Packet> p,
                                Ipv4Header const &ipv4Header, HomaHeader const &homaHeader, 
                                Ptr<Ipv4Interface> iface, uint32_t mtuBytes, uint16_t rttPackets)
{
  NS_LOG_FUNCTION (this);

  m_ipv4Header = ipv4Header;
  m_iface = iface;
    
  m_sport = homaHeader.GetSrcPort ();
  m_dport = homaHeader.GetDstPort ();
  m_txMsgId = homaHeader.GetTxMsgId ();
   
  m_msgSizePkts = homaHeader.GetMsgLen ();
  /*
   * Note that since the Homa header doesn't include the total message size in bytes,
   * we can only estimate it from the provided message size in packets times the 
   * maximum payload size per packet. The error margin in this estimation comes from 
   * the last packet of the message which may be smaller than the maximum allowed payload size.
   */
  m_msgSizeBytes = m_msgSizePkts * (mtuBytes - homaHeader.GetSerializedSize () - ipv4Header.GetSerializedSize ());
  // The remaining undelivered message size equals to the total message size in the beginning
  m_remainingBytes = m_msgSizeBytes - p->GetSize();
  
  // Fill in the packet buffer with place holder (empty) packets and set the received info as false
  for (uint16_t i = 0; i < m_msgSizePkts; i++)
  {
    m_packets.push_back(Create<Packet> ());
    m_receivedPackets.push_back(false);
  } 
  uint16_t pktOffset = homaHeader.GetPktOffset ();
  m_packets[pktOffset] = p;
  m_receivedPackets[pktOffset] = true;
          
  m_rttPackets = rttPackets;
  m_maxGrantableIdx = m_rttPackets;
  m_maxGrantedIdx = 0;
}

HomaInboundMsg::~HomaInboundMsg ()
{
  NS_LOG_FUNCTION_NOARGS ();
}
    
uint32_t HomaInboundMsg::GetRemainingBytes()
{
  return m_remainingBytes;
}
    
uint16_t HomaInboundMsg::GetMsgSizePkts()
{
  return m_msgSizePkts;
}
    
Ipv4Address HomaInboundMsg::GetSrcAddress ()
{
  return m_ipv4Header.GetSource ();
}
    
Ipv4Address HomaInboundMsg::GetDstAddress ()
{
  return m_ipv4Header.GetDestination ();
}

uint16_t HomaInboundMsg::GetSrcPort ()
{
  return m_sport;
}
    
uint16_t HomaInboundMsg::GetDstPort ()
{
  return m_dport;
}
    
uint16_t HomaInboundMsg::GetTxMsgId ()
{
  return m_txMsgId;
}
    
Ipv4Header HomaInboundMsg::GetIpv4Header ()
{
  return m_ipv4Header;
}
    
Ptr<Ipv4Interface> HomaInboundMsg::GetIpv4Interface ()
{
  return m_iface;
}
 
bool HomaInboundMsg::IsFullyGranted ()
{
  return m_maxGrantedIdx >= m_msgSizePkts;
}
    
bool HomaInboundMsg::IsFullyReceived ()
{
  bool allReceived = true;
  for (uint16_t i = 0; i < m_receivedPackets.size(); i++)
  {
    allReceived = allReceived && m_receivedPackets[i];
  }
  return allReceived;
}

/*
 * This method updates the state for an inbound message upon receival of a data packet.
 */
void HomaInboundMsg::ReceiveDataPacket (Ptr<Packet> p, uint16_t pktOffset)
{
  NS_LOG_FUNCTION (this << p << pktOffset);
    
  if (!m_receivedPackets[pktOffset])
  {
    m_packets[pktOffset] = p;
    m_receivedPackets[pktOffset] = true;
      
    m_remainingBytes -= p->GetSize ();
    /*
     * Since a packet has arrived, we can allow a new packet to be on flight
     * for this message. However it is upto the HomaRecvScheduler to decide 
     * whether to send a Grant packet to the sender of this message or not.
     */
    m_maxGrantableIdx++;
  }
  else
  {
    NS_LOG_WARN("HomaInboundMsg (" << this << ") has received a packet for offset "
                << pktOffset << " which was already received.");
    // TODO: How do we manage the GrantOffset if this was a spurious retransmission?
  }
}
    
Ptr<Packet> HomaInboundMsg::GetReassembledMsg ()
{
  NS_LOG_FUNCTION (this);
    
  Ptr<Packet> completeMsg = Create<Packet> ();
  for (std::size_t i = 0; i < m_packets.size(); i++)
  {
    NS_ASSERT_MSG(m_receivedPackets[i],
                  "ERROR: HomaRecvScheduler is trying to reassemble an incomplete msg!");
    completeMsg->AddAtEnd (m_packets[i]);
  }
  
  return completeMsg;
}
    
/******************************************************************************/

TypeId HomaRecvScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaRecvScheduler")
    .SetParent<Object> ()
    .SetGroupName("Internet")
  ;
  return tid;
}
    
HomaRecvScheduler::HomaRecvScheduler (Ptr<HomaL4Protocol> homaL4Protocol)
{
  NS_LOG_FUNCTION (this);
      
  m_homa = homaL4Protocol;
}

HomaRecvScheduler::~HomaRecvScheduler ()
{
  NS_LOG_FUNCTION_NOARGS ();
}
 
/*
 * This method is called in the beginning of the simulation (inside 
 * NotifyNewAggregate()) to set up the correct topological information 
 * inside the Homa protocol logic. 
 */
void HomaRecvScheduler::SetMtuAndBdp (uint32_t mtuBytes, uint16_t rttPackets)
{
  NS_LOG_FUNCTION (this << mtuBytes << rttPackets);
    
  m_mtuBytes = mtuBytes;
  m_rttPackets = rttPackets;
}
    
void HomaRecvScheduler::ReceiveDataPacket (Ptr<Packet> packet, 
                                           Ipv4Header const &ipv4Header,
                                           HomaHeader const &homaHeader,
                                           Ptr<Ipv4Interface> interface)
{
  NS_LOG_FUNCTION (this << ipv4Header << homaHeader);
    
  Ptr<Packet> cp = packet->Copy();
    
  Ptr<HomaInboundMsg> inboundMsg;
  int activeMsgIdx = -1;
  if (this->GetInboundMsg(ipv4Header, homaHeader, inboundMsg, activeMsgIdx))
  {
    inboundMsg->ReceiveDataPacket (cp, homaHeader.GetPktOffset());
    this->RescheduleMsg (inboundMsg, activeMsgIdx);
  }
  else
  {
    inboundMsg = CreateObject<HomaInboundMsg> (cp, ipv4Header, homaHeader, 
                                               interface, m_mtuBytes, m_rttPackets);
    /*
     * Even if the message was a single packet one and now is fully received,
     * we should still schedule it first because ScheduleNewMsg also lets 
     * HomaRecvScheduler know that the sender is not busy anymore.
     */
    this->ScheduleNewMsg(inboundMsg);
  }
    
  if (inboundMsg->IsFullyReceived ())
  {
    NS_LOG_LOGIC("HomaInboundMsg (" << inboundMsg <<
                 ") is fully received. Forwarding the message to applications.");
    // Once the message fowarded, it will also be removed from the active msg list
    this->ForwardUp (inboundMsg);
  }
    
  // TODO: Send Grant to the inboundMsg if remainingBytes allows it to be
  //       one of the granted messages.
}
    
bool HomaRecvScheduler::GetInboundMsg(Ipv4Header const &ipv4Header, 
                                      HomaHeader const &homaHeader, 
                                      Ptr<HomaInboundMsg> &inboundMsg,
                                      int &activeMsgIdx)
{
  NS_LOG_FUNCTION (this << ipv4Header << homaHeader);
    
  Ptr<HomaInboundMsg> currentMsg;
  // First look for the message in the list of active messages
  for (std::size_t i = 0; i < m_activeInboundMsgs.size(); ++i) 
  {
    currentMsg = m_activeInboundMsgs[i];
    if (currentMsg->GetSrcAddress() == ipv4Header.GetSource() &&
        currentMsg->GetDstAddress() == ipv4Header.GetDestination() &&
        currentMsg->GetSrcPort() == homaHeader.GetSrcPort() &&
        currentMsg->GetDstPort() == homaHeader.GetDstPort() &&
        currentMsg->GetTxMsgId() == homaHeader.GetTxMsgId())
    {
      NS_LOG_LOGIC("The HomaInboundMsg (" << currentMsg
                   << ") is found among the active messages.");
      inboundMsg = currentMsg;
      activeMsgIdx = i;
      return true;
    }
  }
    
  // If the message is not found above, it might be marked as busy
  auto busyMsgs = m_busyInboundMsgs.find(ipv4Header.GetSource().Get());
  if (busyMsgs != m_busyInboundMsgs.end())
  {
    for (std::size_t i = 0; i < busyMsgs->second.size(); ++i) 
    {
      currentMsg = busyMsgs->second[i];
      if (currentMsg->GetSrcAddress() == ipv4Header.GetSource() &&
          currentMsg->GetDstAddress() == ipv4Header.GetDestination() &&
          currentMsg->GetSrcPort() == homaHeader.GetSrcPort() &&
          currentMsg->GetDstPort() == homaHeader.GetDstPort() &&
          currentMsg->GetTxMsgId() == homaHeader.GetTxMsgId())
      {
        NS_LOG_LOGIC("The HomaInboundMsg (" << currentMsg
                     << ") is found among the messages that were marked as busy.");
        inboundMsg = currentMsg;
        return true;
      }
    }
  }
  
  NS_LOG_LOGIC("Incoming packet doesn't belong to an existing inbound message.");
  return false;
}
    
void HomaRecvScheduler::ScheduleNewMsg(Ptr<HomaInboundMsg> inboundMsg)
{
  NS_LOG_FUNCTION (this << inboundMsg);
    
  bool msgInserted = false;
  for(std::size_t i = 0; i < m_activeInboundMsgs.size(); ++i) 
  {
    if(inboundMsg->GetRemainingBytes () < m_activeInboundMsgs[i]->GetRemainingBytes())
    {
      m_activeInboundMsgs.insert(m_activeInboundMsgs.begin()+i, inboundMsg);
      msgInserted = true;
      break;
    }
  }
  if (!msgInserted)
  {
    // The remaining size of the inboundMsg is larger than all the active messages
    m_activeInboundMsgs.push_back(inboundMsg);
  }
    
  /*
   * Scheduling a new message means the sender is not busy anymore, so we should 
   * try to schedule the pending inbound messages that were previously marked as busy
   * from the same sender.
   */
  this->SchedulePreviouslyBusySender(inboundMsg->GetSrcAddress().Get());
}
    
/*
 * This method is called after the corresponding inbound message is found among the
 * pending messages. This means if the message was found to be active, its index (order)
 * was also detected. Then this method takes this index as an optimization which
 * prevents looping through all the active messages once again. 
 */
void HomaRecvScheduler::RescheduleMsg (Ptr<HomaInboundMsg> inboundMsg, 
                                       int activeMsgIdx)
{
  NS_LOG_FUNCTION (this << inboundMsg << activeMsgIdx);
    
  if (activeMsgIdx >= 0)
  {
    NS_LOG_LOGIC("The HomaInboundMsg (" << inboundMsg 
                 << ") is already an active message. Reordering it.");
    
    // Make sure the activeMsgIdx matches inboundMsg
    Ptr<HomaInboundMsg> msgToReschedule = m_activeInboundMsgs[activeMsgIdx];
    NS_ASSERT(msgToReschedule->GetSrcAddress() == inboundMsg->GetSrcAddress() &&
              msgToReschedule->GetDstAddress() == inboundMsg->GetDstAddress() &&
              msgToReschedule->GetSrcPort() == inboundMsg->GetSrcPort() &&
              msgToReschedule->GetDstPort() == inboundMsg->GetDstPort() &&
              msgToReschedule->GetTxMsgId() == inboundMsg->GetTxMsgId());
    
    // Remove the corresponding message and re-insert with the correct ordering
    m_activeInboundMsgs.erase(m_activeInboundMsgs.begin() + activeMsgIdx);
    this->ScheduleNewMsg(inboundMsg);
  }
  else
  {
    uint32_t senderIP = inboundMsg->GetSrcAddress().Get();
    NS_LOG_LOGIC("The sender (" << senderIP 
                 << ") was previously marked as Busy."
                 << " Scheduling all pending messages from this sender.");
    this->SchedulePreviouslyBusySender(senderIP);
  }
}
 
/*
 * This method takes the first busy inbound message of the sender and 
 * schedules it (ie, inserts it into the list of active messages). The 
 * ScheduleNewMsg method calls this one again until the all busy messages
 * of the same sender are scheduled one by one.
 */
void HomaRecvScheduler::SchedulePreviouslyBusySender(uint32_t senderIP)
{
  NS_LOG_FUNCTION (this << senderIP);
    
  Ptr<HomaInboundMsg> previouslyBusyMsg;
  auto busyMsgs = m_busyInboundMsgs.find(senderIP);
  if (busyMsgs != m_busyInboundMsgs.end())
  {
    previouslyBusyMsg = busyMsgs->second[0]; // Get the message at the head of the vector
    busyMsgs->second.erase(busyMsgs->second.begin()); // Remove the head from the vector
    if (busyMsgs->second.size() == 0)
    {
      m_busyInboundMsgs.erase(busyMsgs); // Delete the entry for the sender because it is empty
    }
      
    this->ScheduleNewMsg(previouslyBusyMsg);
  } 
}
    
void HomaRecvScheduler::ForwardUp(Ptr<HomaInboundMsg> inboundMsg)
{
  NS_LOG_FUNCTION (this << inboundMsg);
    
  m_homa->ForwardUp (inboundMsg->GetReassembledMsg(), 
                     inboundMsg->GetIpv4Header (),
                     inboundMsg->GetSrcPort(),
                     inboundMsg->GetDstPort(),
                     inboundMsg->GetIpv4Interface());
    
  /*
   * Since the message is being forwarded up, it should have been 
   * received completely which means the sender is not marked as busy.
   * Therefore the completed message should be listed on the active 
   * messages list.
   */
  this->RemoveMsgFromActiveMsgsList (inboundMsg);
}
    
void HomaRecvScheduler::RemoveMsgFromActiveMsgsList(Ptr<HomaInboundMsg> inboundMsg)
{
  NS_LOG_FUNCTION (this << inboundMsg);
  
  bool msgRemoved = false;
  Ptr<HomaInboundMsg> currentMsg;
  // First look for the message in the list of active messages
  for (std::size_t i = 0; i < m_activeInboundMsgs.size(); ++i) 
  {
    currentMsg = m_activeInboundMsgs[i];
    if (currentMsg->GetSrcAddress() == inboundMsg->GetSrcAddress() &&
        currentMsg->GetDstAddress() == inboundMsg->GetDstAddress() &&
        currentMsg->GetSrcPort() == inboundMsg->GetSrcPort() &&
        currentMsg->GetDstPort() == inboundMsg->GetDstPort() &&
        currentMsg->GetTxMsgId() == inboundMsg->GetTxMsgId())
    {
      NS_LOG_DEBUG("Erasing HomaInboundMsg (" << inboundMsg << 
                   ") from the active messages list of HomaRecvScheduler (" << 
                   this << ").");
      m_activeInboundMsgs.erase(m_activeInboundMsgs.begin()+i);
      msgRemoved = true;
      break;
    }
  }
    
  if (!msgRemoved)
  {
    NS_LOG_ERROR("ERROR: HomaInboundMsg (" << inboundMsg <<
                 ") couldn't be find inside the active messages list!");
  }
}
    
} // namespace ns3