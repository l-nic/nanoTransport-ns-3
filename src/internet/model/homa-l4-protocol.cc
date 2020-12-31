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
    .AddAttribute ("NumTotalPrioBands", "Total number of priority levels used within the network",
                   UintegerValue (8),
                   MakeUintegerAccessor (&HomaL4Protocol::m_numTotalPrioBands),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("NumUnschedPrioBands", "Number of priority bands dedicated for unscheduled packets",
                   UintegerValue (2),
                   MakeUintegerAccessor (&HomaL4Protocol::m_numUnschedPrioBands),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("OvercommitLevel", "Minimum number of messages to Grant at the same time",
                   UintegerValue (6),
                   MakeUintegerAccessor (&HomaL4Protocol::m_overcommitLevel),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("InbndRtxTimeout", "Time value to determine the retransmission timeout of InboundMsgs",
                   TimeValue (MicroSeconds (50)),
                   MakeTimeAccessor (&HomaL4Protocol::m_inboundRtxTimeout),
                   MakeTimeChecker (MicroSeconds (0)))
    .AddAttribute ("OutbndRtxTimeout", "Time value to determine the timeout of OutboundMsgs",
                   TimeValue (MilliSeconds (5)),
                   MakeTimeAccessor (&HomaL4Protocol::m_outboundRtxTimeout),
                   MakeTimeChecker (MicroSeconds (0)))
    .AddAttribute ("MaxRtxCnt", "Maximum allowed consecutive rtx timeout count per message",
                   UintegerValue (5),
                   MakeUintegerAccessor (&HomaL4Protocol::m_maxNumRtxPerMsg),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("MsgBegin",
                     "Trace source indicating a message has been delivered to "
                     "the HomaL4Protocol by the sender application layer.",
                     MakeTraceSourceAccessor (&HomaL4Protocol::m_msgBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MsgFinish",
                     "Trace source indicating a message has been delivered to "
                     "the receiver application by the HomaL4Protocol layer.",
                     MakeTraceSourceAccessor (&HomaL4Protocol::m_msgFinishTrace),
                     "ns3::Packet::TracedCallback")
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
    
  Ptr<NetDevice> netDevice = m_node->GetDevice (0);
  m_mtu = netDevice->GetMtu ();
    
  PointToPointNetDevice* p2pNetDevice = dynamic_cast<PointToPointNetDevice*>(&(*(netDevice)));
  m_linkRate = p2pNetDevice->GetDataRate ();
    
  m_nextTimeTxQueWillBeEmpty = Simulator::Now ();
}
    
Ptr<Node> 
HomaL4Protocol::GetNode(void) const
{
  return m_node;
}
    
uint32_t
HomaL4Protocol::GetMtu (void) const
{
  return m_mtu;
}
    
uint16_t 
HomaL4Protocol::GetBdp(void) const
{
  return m_bdp;
}
    
int 
HomaL4Protocol::GetProtocolNumber (void) const
{
  return PROT_NUMBER;
}
 
Time
HomaL4Protocol::GetInboundRtxTimeout(void) const
{
  return m_inboundRtxTimeout;
}
    
uint16_t 
HomaL4Protocol::GetMaxNumRtxPerMsg(void) const
{
  return m_maxNumRtxPerMsg;
}
    
uint8_t
HomaL4Protocol::GetNumTotalPrioBands (void) const
{
  return m_numTotalPrioBands;
}
    
uint8_t
HomaL4Protocol::GetNumUnschedPrioBands (void) const
{
  return m_numUnschedPrioBands;
}
    
uint8_t
HomaL4Protocol::GetOvercommitLevel (void) const
{
  return m_overcommitLevel;
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
          
          NS_ASSERT(m_mtu); // m_mtu is set inside SetNode() above.
          NS_ASSERT_MSG(m_numTotalPrioBands > m_numUnschedPrioBands,
                "Total number of priority bands should be larger than the number of bands dedicated for unscheduled packets.");
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
                                                               this->GetMtu(), m_bdp);
  outMsg->SetRoute (route); // This is mostly unnecessary
  outMsg->SetRtxTimeout (m_outboundRtxTimeout);
  int txMsgId = m_sendScheduler->ScheduleNewMsg(outMsg);
    
  if (txMsgId >= 0)
    m_msgBeginTrace(message, saddr, daddr, sport, dport, txMsgId);
}

/*
 * This method is called either by the associated HomaSendScheduler after 
 * the next data packet to transmit is selected or by the associated
 * HomaRecvScheduler once a control packet is generated. The selected 
 * packet is then pushed down to the lower IP layer.
 */
void
HomaL4Protocol::SendDown (Ptr<Packet> packet, 
                          Ipv4Address saddr, Ipv4Address daddr, 
                          Ptr<Ipv4Route> route)
{
  NS_LOG_FUNCTION (this << packet << saddr << daddr << route);
    
  PppHeader pph;
  Ipv4Header iph;
  uint32_t headerSize = iph.GetSerializedSize () + pph.GetSerializedSize ();  
  Time timeToSerialize = m_linkRate.CalculateBytesTxTime (packet->GetSize () + headerSize);
    
  if(Simulator::Now() < m_nextTimeTxQueWillBeEmpty)
  {
    m_nextTimeTxQueWillBeEmpty += timeToSerialize;
  }
  else
  {
    m_nextTimeTxQueWillBeEmpty = Simulator::Now() + timeToSerialize;
  }
   
  m_downTarget (packet, saddr, daddr, PROT_NUMBER, route);
}
    
Time HomaL4Protocol::GetTimeToDrainTxQueue ()
{
  NS_LOG_FUNCTION(this);
    
  if(Simulator::Now() < m_nextTimeTxQueWillBeEmpty)
  {
    return m_nextTimeTxQueWillBeEmpty - Simulator::Now();
  }
  else
  {
    return Time(0);
  }
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
    
  NS_LOG_DEBUG ("HomaL4Protocol (" << this << ") received: " << packet->ToString ());
    
  NS_ASSERT(header.GetProtocol() == PROT_NUMBER);
  
  Ptr<Packet> cp = packet->Copy ();
    
  HomaHeader homaHeader;
  cp->RemoveHeader(homaHeader);
  NS_ASSERT_MSG(cp->GetSize()==homaHeader.GetPayloadSize(),
                "HomaL4Protocol (" << this << ") received a packet "
                " whose payload size doesn't match the homa header field!");

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
  if (rxFlag & HomaHeader::Flags_t::DATA ||
      rxFlag & HomaHeader::Flags_t::BUSY)
  {
    m_recvScheduler->ReceivePacket(cp, header, homaHeader, interface);
  }
  else if ((rxFlag & HomaHeader::Flags_t::GRANT) ||
           (rxFlag & HomaHeader::Flags_t::RESEND) ||
           (rxFlag & HomaHeader::Flags_t::ACK))
  {
    m_sendScheduler->CtrlPktRecvdForOutboundMsg(header, homaHeader);
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
                                uint16_t sport, uint16_t dport, uint16_t txMsgId,
                                Ptr<Ipv4Interface> incomingInterface)
{
  NS_LOG_FUNCTION (this << completeMsg << header << sport << incomingInterface);
    
  NS_LOG_DEBUG ("Looking up dst " << header.GetDestination () << " port " << dport); 
  Ipv4EndPointDemux::EndPoints endPoints =
    m_endPoints->Lookup (header.GetDestination (), dport,
                         header.GetSource (), sport, incomingInterface);
    
  NS_ASSERT_MSG(!endPoints.empty (), 
                "HomaL4Protocol was able to find an endpoint when msg was received, but now it couldn't");
    
  for (Ipv4EndPointDemux::EndPointsI endPoint = endPoints.begin ();
         endPoint != endPoints.end (); endPoint++)
  {
    (*endPoint)->ForwardUp (completeMsg, header, sport, incomingInterface);
  }
    
  m_msgFinishTrace(completeMsg, header.GetSource(), header.GetDestination(), 
                   sport, dport, (int)txMsgId);
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
      m_prioSetByReceiver(false),
      m_isExpired(false)
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
    nextPkt = message->CreateFragment (m_msgSizeBytes - unpacketizedBytes, nextPktSize);

    m_packets.push_back(nextPkt);
    m_pktTxQ.push(numPkts);

    unpacketizedBytes -= nextPktSize;
    numPkts++;
  } 
  NS_ASSERT(numPkts == m_msgSizeBytes / m_maxPayloadSize + (m_msgSizeBytes % m_maxPayloadSize != 0));
          
  m_rttPackets = rttPackets;
  m_maxGrantedIdx = std::min((uint16_t)(m_rttPackets-1), numPkts);
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
    
void HomaOutboundMsg::SetRtxTimeout (Time rtxTimeout)
{
  NS_LOG_FUNCTION(this << rtxTimeout);
    
  m_rtxTimeout = rtxTimeout;
  m_rtxEvent = Simulator::Schedule (m_rtxTimeout, &HomaOutboundMsg::ExpireRtxTimeout, 
                                    this, m_maxGrantedIdx);
}
    
uint32_t HomaOutboundMsg::GetRemainingBytes()
{
  return m_remainingBytes;
}

uint32_t HomaOutboundMsg::GetMsgSizeBytes()
{
  return m_msgSizeBytes;
}
uint16_t HomaOutboundMsg::GetMsgSizePkts()
{
  return m_msgSizeBytes / m_maxPayloadSize + (m_msgSizeBytes % m_maxPayloadSize != 0);
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
    
bool HomaOutboundMsg::IsExpired ()
{
  return m_isExpired;
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
    
EventId HomaOutboundMsg::GetRtxEvent ()
{
  return m_rtxEvent;
}
    
bool HomaOutboundMsg::GetNextPktOffset (uint16_t &pktOffset)
{
  NS_LOG_FUNCTION (this);
    
  if (!m_pktTxQ.empty ())
  {
    uint16_t nextPktOffset = m_pktTxQ.top();
    
    if (nextPktOffset <= m_maxGrantedIdx && nextPktOffset < this->GetMsgSizePkts ())
    {
      // The selected packet is not delivered and not on flight
      NS_LOG_LOGIC("HomaOutboundMsg (" << this 
                   << ") can send packet " << nextPktOffset << " next.");
      pktOffset = nextPktOffset;
      return true;
    }
  }
  NS_LOG_LOGIC("HomaOutboundMsg (" << this 
               << ") doesn't have any packet to send!");
  return false;
}
    
Ptr<Packet> HomaOutboundMsg::RemoveNextPktFromTxQ (uint16_t pktOffset)
{
  NS_LOG_FUNCTION (this << pktOffset);
    
  NS_ASSERT_MSG(!m_pktTxQ.empty (), 
                "HomaOutboundMsg can't send a pkt if its TX queue is empty!");
  NS_ASSERT_MSG(m_pktTxQ.top() == pktOffset,
                "HomaOutboundMsg can only send the packet at the head of TX queue!");
  
  /*
   * In case a pktOffset was added multiple times to the tx queue,
   * we remove all of them at once to prevent redundant transmissions.
   */
  while(m_pktTxQ.top() == pktOffset && !m_pktTxQ.empty ())
  {
    m_pktTxQ.pop();
  }
    
  return m_packets[pktOffset]->Copy();
}
    
/*
 * This method updates the state for the corresponding outbound message
 * upon receival of a Grant or RESEND. The state is updated only if the  
 * granted packet index is larger than the highest grant index received 
 * so far. This allows reordered Grants to be ignored when more recent 
 * ones are received.
 */
void HomaOutboundMsg::HandleGrantOffset (HomaHeader const &homaHeader)
{
  NS_LOG_FUNCTION (this << homaHeader);
    
  uint16_t grantOffset = homaHeader.GetGrantOffset();
  NS_ASSERT_MSG(grantOffset <= this->GetMsgSizePkts (), 
                "HomaOutboundMsg shouldn't be granted after it is already fully granted!");
    
  if (grantOffset >= m_maxGrantedIdx)
  {
    NS_LOG_LOGIC("HomaOutboundMsg (" << this 
                 << ") is increasing the Grant index to "
                 << grantOffset << ".");
      
    m_maxGrantedIdx = grantOffset;
      
    uint8_t prio = homaHeader.GetPrio();
    if (prio < m_prio || !m_prioSetByReceiver)
    {
      NS_LOG_LOGIC("HomaOutboundMsg (" << this 
                   << ") is setting priority to "
                   << (uint16_t) prio << ".");
        
      m_prio = prio;
      m_prioSetByReceiver = true;
    }
      
    /*
     * Since Homa doesn't explicitly acknowledge the delivery of data packets,
     * one way to estimate the remaining bytes is to exploit the mechanism where
     * Homa grants messages in a way that there is always exactly 1 BDP worth of 
     * packets on flight. Then we can calculate the remaining bytes as the following.
     */
    m_remainingBytes = m_msgSizeBytes - (m_maxGrantedIdx - m_rttPackets) * m_maxPayloadSize;
  }
  else
  {
    NS_LOG_LOGIC("HomaOutboundMsg (" << this 
                 << ") has received an out-of-order Grant. State is not updated!");
  }
}
    
void HomaOutboundMsg::HandleResend (HomaHeader const &homaHeader)
{
  NS_LOG_FUNCTION (this << homaHeader);
    
  NS_ASSERT(homaHeader.GetFlags() & HomaHeader::Flags_t::RESEND);
  
  m_pktTxQ.push(homaHeader.GetPktOffset ());
}
    
void HomaOutboundMsg::HandleAck (HomaHeader const &homaHeader)
{
  NS_LOG_FUNCTION (this << homaHeader);
    
  NS_ASSERT(homaHeader.GetFlags() & HomaHeader::Flags_t::ACK);
    
  NS_ASSERT(homaHeader.GetPktOffset () == this->GetMsgSizePkts ());
  m_remainingBytes = 0;
}
    
Ptr<Packet> HomaOutboundMsg::GenerateBusy (uint16_t targetTxMsgId)
{
  NS_LOG_FUNCTION (this);
  
  HomaHeader homaHeader;
  homaHeader.SetSrcPort (m_sport); 
  homaHeader.SetDstPort (m_dport);
  homaHeader.SetTxMsgId (targetTxMsgId);
  homaHeader.SetMsgSize (m_msgSizeBytes);
//   homaHeader.SetPktOffset (0); // TODO: Is this correct?
  homaHeader.SetGrantOffset (m_maxGrantedIdx); // TODO: Is this correct?
  homaHeader.SetPrio (m_prio); // TODO: Is this correct?
  homaHeader.SetPayloadSize (0);
  homaHeader.SetFlags (HomaHeader::Flags_t::BUSY);
    
  Ptr<Packet> busyPacket = Create<Packet> ();
  busyPacket->AddHeader (homaHeader);
    
  SocketIpTosTag ipTosTag;
  ipTosTag.SetTos (0); // Busy packets have the highest priority
  // This packet may already have a SocketIpTosTag (see HomaSocket)
  busyPacket->ReplacePacketTag (ipTosTag);
    
  return busyPacket;
}
    
void HomaOutboundMsg::ExpireRtxTimeout(uint16_t lastRtxGrntIdx)
{
  NS_LOG_FUNCTION(this << lastRtxGrntIdx);
    
  if (m_remainingBytes == 0) // Fully delivered (ACK received)
  {
    return;
  }
  
  if (lastRtxGrntIdx < m_maxGrantedIdx)
  {
    m_rtxEvent = Simulator::Schedule (m_rtxTimeout, &HomaOutboundMsg::ExpireRtxTimeout, 
                                      this, m_maxGrantedIdx);
  }
  else
  {
    NS_LOG_WARN("HomaOutboundMsg (" << this << ") has timed-out.");
    m_isExpired = true;
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
    
  int numIncmpltMsg = m_outboundMsgs.size();
  if (numIncmpltMsg > 0)
  {
    NS_LOG_ERROR("ERROR: HomaSendScheduler (" << this <<
                 ") couldn't completely deliver " << 
                 numIncmpltMsg << " outbound messages!");
  }
}

/*
 * This method is called upon receiving a new message from the application layer.
 * It inserts the message into the list of pending outbound messages and updates
 * the scheduler's state accordingly.
 */
int HomaSendScheduler::ScheduleNewMsg (Ptr<HomaOutboundMsg> outMsg)
{
  NS_LOG_FUNCTION (this << outMsg);
    
  uint16_t txMsgId;
  if (m_txMsgIdFreeList.size() > 0)
  {
    // Assign a unique txMsgId which will persist while the message lasts
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
      m_txEvent = Simulator::Schedule (m_homa->GetTimeToDrainTxQueue(), 
                                       &HomaSendScheduler::TxDataPacket, this);
  }
  else
  {
    NS_LOG_ERROR(Simulator::Now ().GetNanoSeconds () << 
                 " Error: HomaSendScheduler ("<< this << 
                 ") could not allocate a new txMsgId for message (" << 
                 outMsg << ")" );
    return -1;
  }
  return (int)txMsgId;
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
  std::list<uint16_t> expiredMsgIds;
  uint32_t minRemainingBytes = std::numeric_limits<uint32_t>::max();
  uint16_t pktOffset;
  bool msgSelected = false;
  /*
   * Iterate over all pending outbound messages and select the one 
   * that has the smallest remainingBytes, granted but not transmitted 
   * packets, and a receiver that is not listed as busy.
   */
  for (auto& it: m_outboundMsgs) 
  {
    currentMsg = it.second;
      
    if (!currentMsg->IsExpired ())
    { 
      uint32_t curRemainingBytes = currentMsg->GetRemainingBytes();
      // Accept current msg if it has a granted but not transmitted packet
      if (currentMsg->GetNextPktOffset(pktOffset))
      {
        // Accept current msg if the remainig size is smaller than minRemainingBytes
        if (curRemainingBytes < minRemainingBytes)
        {
          candidateMsg = currentMsg;
          txMsgId = it.first;
          minRemainingBytes = curRemainingBytes;
          msgSelected = true;
        }
      }
    }
    else // Expired messages should be removed from the state
    {
      expiredMsgIds.push_back(it.first); 
    }
  }
    
  /*
   * We only record the txMsgId of the expired msgs above and only remove them
   * after the for loop above finishes because the ClearStateForMsg function
   * also iterates over m_outboundMsgs and erases an element which may cause the
   * iterator to skip elements in the for loop above.
   */
  for (const auto& expiredTxMsgId : expiredMsgIds)
  {
    this->ClearStateForMsg (expiredTxMsgId);
  }
    
  return msgSelected;
}
    
bool HomaSendScheduler::GetNextPktOfMsg (uint16_t txMsgId, Ptr<Packet> &p)
{
  NS_LOG_FUNCTION (this << txMsgId);
  
  uint16_t pktOffset;
  Ptr<HomaOutboundMsg> candidateMsg = m_outboundMsgs[txMsgId];
    
  if (candidateMsg->GetNextPktOffset(pktOffset))
  {
    p = candidateMsg->RemoveNextPktFromTxQ(pktOffset);
      
    HomaHeader homaHeader;
    homaHeader.SetDstPort (candidateMsg->GetDstPort ());
    homaHeader.SetSrcPort (candidateMsg->GetSrcPort ());
    homaHeader.SetTxMsgId (txMsgId);
    homaHeader.SetFlags (HomaHeader::Flags_t::DATA); 
    homaHeader.SetMsgSize (candidateMsg->GetMsgSizeBytes ());
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
    
    return true;
  }
  else
  {
    return false;
  }
}
 
/*
 * This method is called either when a new packet to send is found after
 * an idle time period or when the serialization of the previous packets 
 * finish. This allows HomaSendScheduler to choose the most recent highest 
 * priority packet just before sending it.
 */
void
HomaSendScheduler::TxDataPacket ()
{
  NS_LOG_FUNCTION (this);
    
  NS_ASSERT(m_txEvent.IsExpired());
    
  Time timeToDrainTxQ = m_homa->GetTimeToDrainTxQueue();
  if (timeToDrainTxQ != Time(0))
  {
    m_txEvent = Simulator::Schedule (timeToDrainTxQ, 
                                     &HomaSendScheduler::TxDataPacket, this);
    return;
  }
    
  uint16_t nextTxMsgID;
  Ptr<Packet> p;
  if (this->GetNextMsgId (nextTxMsgID))
  {   
    NS_ASSERT(this->GetNextPktOfMsg(nextTxMsgID, p));
      
    NS_LOG_LOGIC("HomaSendScheduler (" << this <<
                  ") will transmit a packet from msg " << nextTxMsgID);
      
    Ptr<HomaOutboundMsg> nextMsg = m_outboundMsgs[nextTxMsgID];
    Ipv4Address saddr = nextMsg->GetSrcAddress ();
    Ipv4Address daddr = nextMsg->GetDstAddress ();
    Ptr<Ipv4Route> route = nextMsg->GetRoute ();
    
    m_homa->SendDown(p, saddr, daddr, route);
    
    m_txEvent = Simulator::Schedule (m_homa->GetTimeToDrainTxQueue(), 
                                     &HomaSendScheduler::TxDataPacket, this);
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
    
  uint16_t targetTxMsgId = homaHeader.GetTxMsgId();
  if(m_outboundMsgs.find(targetTxMsgId) == m_outboundMsgs.end())
  {
    NS_LOG_ERROR("ERROR: HomaSendScheduler (" << this <<
                 ") received a " << homaHeader.FlagsToString(homaHeader.GetFlags()) << 
                 " packet for an unknown txMsgId (" << 
                 targetTxMsgId << ").");
    return;
  }
    
  Ptr<HomaOutboundMsg> targetMsg = m_outboundMsgs[targetTxMsgId];
  // Verify that the TxMsgId indeed matches the 4 tuple
  NS_ASSERT( (targetMsg->GetSrcAddress() == ipv4Header.GetDestination ()) &&
             (targetMsg->GetDstAddress() == ipv4Header.GetSource ()) && 
             (targetMsg->GetSrcPort() == homaHeader.GetDstPort ()) &&
             (targetMsg->GetDstPort() == homaHeader.GetSrcPort ()) );
  
  uint8_t ctrlFlag = homaHeader.GetFlags();
  if (ctrlFlag & HomaHeader::Flags_t::GRANT)
  {
    targetMsg->HandleGrantOffset (homaHeader);
  }
  else if (ctrlFlag & HomaHeader::Flags_t::RESEND)
  {
    targetMsg->HandleGrantOffset (homaHeader);
    targetMsg->HandleResend (homaHeader);
      
    uint16_t nextTxMsgID;
    this->GetNextMsgId (nextTxMsgID);
    if (nextTxMsgID != targetTxMsgId) 
    {
      // Incoming packet doesn't belong to the highest priority outboung message.
      NS_LOG_LOGIC("HomaSendScheduler (" << this 
                   << ") needs to send a BUSY packet for " << targetTxMsgId);
      
      m_homa->SendDown(targetMsg->GenerateBusy (nextTxMsgID), 
                       targetMsg->GetSrcAddress (), 
                       targetMsg->GetDstAddress (), 
                       targetMsg->GetRoute ());
    }
  }
  else if (ctrlFlag & HomaHeader::Flags_t::ACK)
  {
    NS_LOG_LOGIC("The HomaOutboundMsg (" << targetMsg << ") is fully delivered!");
    
    targetMsg->HandleAck (homaHeader); // Asserts some sanity checks.
    this->ClearStateForMsg (targetTxMsgId);
  }
  else
  {
    NS_LOG_ERROR("ERROR: HomaSendScheduler (" << this 
                 << ") has received an unexpected control packet ("
                 << homaHeader.FlagsToString(ctrlFlag) << ")");
      
    return;
  }
    
  /* 
   * Since control packets may allow new packets to be sent, we should try 
   * to transmit those packets.
   */
  if(m_txEvent.IsExpired()) 
    m_txEvent = Simulator::Schedule (m_homa->GetTimeToDrainTxQueue(), 
                                     &HomaSendScheduler::TxDataPacket, this);
}
    
void HomaSendScheduler::ClearStateForMsg (uint16_t txMsgId)
{
  NS_LOG_FUNCTION(this << txMsgId);
  
  Simulator::Cancel (m_outboundMsgs[txMsgId]->GetRtxEvent ());
  m_outboundMsgs.erase(m_outboundMsgs.find(txMsgId));
  m_txMsgIdFreeList.push_back(txMsgId);
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
    : m_prio(0),
      m_numRtxWithoutProgress (0)
{
  NS_LOG_FUNCTION (this);

  m_ipv4Header = ipv4Header;
  m_iface = iface;
    
  m_sport = homaHeader.GetSrcPort ();
  m_dport = homaHeader.GetDstPort ();
  m_txMsgId = homaHeader.GetTxMsgId ();
   
  m_msgSizeBytes = homaHeader.GetMsgSize ();
  uint32_t maxPayloadSize = mtuBytes - homaHeader.GetSerializedSize () - ipv4Header.GetSerializedSize ();
  m_msgSizePkts = m_msgSizeBytes / maxPayloadSize + (m_msgSizeBytes % maxPayloadSize != 0);
          
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
          
  m_maxGrantedIdx = std::min((uint16_t)(rttPackets-1), m_msgSizePkts); // Unscheduled pkts are already granted
  m_maxGrantableIdx = m_maxGrantedIdx + 1; // 1 Data pkt is already received
  m_lastRtxGrntIdx = m_maxGrantableIdx;
}

HomaInboundMsg::~HomaInboundMsg ()
{
  NS_LOG_FUNCTION_NOARGS ();
}
    
uint32_t HomaInboundMsg::GetRemainingBytes()
{
  return m_remainingBytes;
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

/*
 * Although the retransmission events are handled by the HomaRecvScheduler
 * the corresponding EventId of messages are kept within the messages 
 * themselves for the sake of being tidy.
 */
void HomaInboundMsg::SetRtxEvent (EventId rtxEvent)
{
  m_rtxEvent = rtxEvent;
}
EventId HomaInboundMsg::GetRtxEvent ()
{
  return m_rtxEvent;
}

uint16_t HomaInboundMsg::GetMaxGrantableIdx ()
{
  return m_maxGrantableIdx;
}
uint16_t HomaInboundMsg::GetMaxGrantedIdx ()
{
  return m_maxGrantedIdx;
}
 
void HomaInboundMsg::SetLastRtxGrntIdx (uint16_t lastRtxGrntIdx)
{
  m_lastRtxGrntIdx = lastRtxGrntIdx;
}
uint16_t HomaInboundMsg::GetLastRtxGrntIdx ()
{
  return m_lastRtxGrntIdx;
}
    
bool HomaInboundMsg::IsFullyGranted ()
{
  return m_maxGrantedIdx >= m_msgSizePkts;
}

bool HomaInboundMsg::IsGrantable ()
{
  return m_maxGrantedIdx < m_maxGrantableIdx;
}
    
bool HomaInboundMsg::IsFullyReceived ()
{
//   for (uint16_t i = 0; i < m_msgSizePkts; i++)
//   {
//     if (!m_receivedPackets[i])
//     {
//       return false;
//     }
//   }
//   return true;
  return std::none_of(m_receivedPackets.begin(), 
                      m_receivedPackets.end(), 
                      std::logical_not<bool>());
}
    
uint16_t HomaInboundMsg::GetNumRtxWithoutProgress ()
{
  return m_numRtxWithoutProgress;
}
void HomaInboundMsg::IncrNumRtxWithoutProgress ()
{
  m_numRtxWithoutProgress++;
}
void HomaInboundMsg::ResetNumRtxWithoutProgress ()
{
  m_numRtxWithoutProgress = 0;
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
    // TODO: How do we make sure the bytes in flight is always equal to RttBytes?
    //       For example if a data packet gets dropped, the effective bytes in flight
    //       remains low until the lost packet is retransmitted.
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
  for (std::size_t i = 0; i < m_msgSizePkts; i++)
  {
    NS_ASSERT_MSG(m_receivedPackets[i],
                  "ERROR: HomaRecvScheduler is trying to reassemble an incomplete msg!");
    completeMsg->AddAtEnd (m_packets[i]);
  }
  
  return completeMsg;
}
    
Ptr<Packet> HomaInboundMsg::GenerateGrantOrAck(uint8_t grantedPrio,
                                               uint8_t pktTypeFlag)
{
  NS_LOG_FUNCTION (this << grantedPrio);
  NS_ASSERT(this->IsGrantable());
  NS_ASSERT_MSG((pktTypeFlag & HomaHeader::Flags_t::GRANT) || 
                (pktTypeFlag & HomaHeader::Flags_t::ACK),
                "GenerateGrantOrAck() can only be called to generate GRANT or ACK packets!");
    
  m_prio = grantedPrio; // Updated with the most recent granted priority value
    
  uint16_t ackNo = m_msgSizePkts;
  for (std::size_t i = 0; i < m_msgSizePkts; i++)
  {
    if (!m_receivedPackets[i])
    {
      ackNo = i; // The earliest un-received packet
      break;
    }
  }
    
  HomaHeader homaHeader;
  // Note we swap the src and dst port numbers for reverse direction
  homaHeader.SetSrcPort (m_dport); 
  homaHeader.SetDstPort (m_sport);
  homaHeader.SetTxMsgId (m_txMsgId);
  homaHeader.SetMsgSize (m_msgSizeBytes);
  homaHeader.SetPktOffset (ackNo); // TODO: Is this correct?
  homaHeader.SetGrantOffset (m_maxGrantableIdx);
  homaHeader.SetPrio (m_prio);
  homaHeader.SetPayloadSize (0);
  homaHeader.SetFlags (pktTypeFlag);
  
  Ptr<Packet> p = Create<Packet> ();
  p->AddHeader (homaHeader);
    
  SocketIpTosTag ipTosTag;
  ipTosTag.SetTos (0); // Grant packets have the highest priority
  // This packet may already have a SocketIpTosTag (see HomaSocket)
  p->ReplacePacketTag (ipTosTag);
    
  m_maxGrantedIdx = m_maxGrantableIdx;
    
  return p;
}
    
std::list<Ptr<Packet>> HomaInboundMsg::GenerateResends (uint16_t maxRsndPktOffset)
{
  NS_LOG_FUNCTION (this << maxRsndPktOffset);
    
  std::list<Ptr<Packet>> rsndPkts;
    
  maxRsndPktOffset = std::min(maxRsndPktOffset, m_msgSizePkts);
  maxRsndPktOffset = std::min(maxRsndPktOffset, m_maxGrantedIdx);
  for (uint16_t i = 0; i < maxRsndPktOffset; ++i)
  {
    if(!m_receivedPackets[i])
    {
      HomaHeader homaHeader;
      // Note we swap the src and dst port numbers for reverse direction
      homaHeader.SetSrcPort (m_dport); 
      homaHeader.SetDstPort (m_sport);
      homaHeader.SetTxMsgId (m_txMsgId);
      homaHeader.SetMsgSize (m_msgSizeBytes);
      homaHeader.SetPktOffset (i); // TODO: Is this correct?
      homaHeader.SetGrantOffset (m_maxGrantedIdx);
      homaHeader.SetPrio (m_prio);
      homaHeader.SetPayloadSize (0);
      homaHeader.SetFlags (HomaHeader::Flags_t::RESEND);
  
      Ptr<Packet> p = Create<Packet> ();
      p->AddHeader (homaHeader);
    
      SocketIpTosTag ipTosTag;
      ipTosTag.SetTos (0); // Resend packets have the highest priority
      // This packet may already have a SocketIpTosTag (see HomaSocket)
      p->ReplacePacketTag (ipTosTag);
        
      rsndPkts.push_back(p);
    }
  }
  
    return rsndPkts;
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
    
  int numIncmpltMsg = m_activeInboundMsgs.size();
  if (numIncmpltMsg > 0)
  {
    NS_LOG_ERROR("ERROR: HomaRecvScheduler (" << this <<
                 ") couldn't completely deliver " << 
                 numIncmpltMsg << " active inbound messages!");
  }
}
    
void HomaRecvScheduler::ReceivePacket (Ptr<Packet> packet, 
                                       Ipv4Header const &ipv4Header,
                                       HomaHeader const &homaHeader,
                                       Ptr<Ipv4Interface> interface)
{
  NS_LOG_FUNCTION (this << packet << ipv4Header << homaHeader);
    
  uint8_t rxFlag = homaHeader.GetFlags ();
  if (rxFlag & HomaHeader::Flags_t::DATA )
  {
    this->ReceiveDataPacket (packet, ipv4Header, homaHeader, interface);
  }
  else if (rxFlag & HomaHeader::Flags_t::BUSY)
  {
    this->BusyReceivedForMsg (ipv4Header.GetSource());
  }
    
  this->SendAppropriateGrants();
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
    inboundMsg = CreateObject<HomaInboundMsg> (cp, ipv4Header, homaHeader, interface, 
                                               m_homa->GetMtu (), m_homa->GetBdp ());
    inboundMsg-> SetRtxEvent (Simulator::Schedule (m_homa->GetInboundRtxTimeout(), 
                                                   &HomaRecvScheduler::ExpireRtxTimeout, this, 
                                                   inboundMsg, inboundMsg->GetMaxGrantedIdx ()));
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
void HomaRecvScheduler::SchedulePreviouslyBusySender(uint32_t senderIp)
{
  NS_LOG_FUNCTION (this << senderIp);
    
  Ptr<HomaInboundMsg> previouslyBusyMsg;
  auto busyMsgsOfSender = m_busyInboundMsgs.find(senderIp);
  if (busyMsgsOfSender != m_busyInboundMsgs.end())
  {
    if (busyMsgsOfSender->second.size() > 0)
    {
      previouslyBusyMsg = busyMsgsOfSender->second[0]; // Get the message at the head of the vector
      
      busyMsgsOfSender->second.erase(busyMsgsOfSender->second.begin()); // Remove the head from the vector
      if (busyMsgsOfSender->second.size() == 0)
      {
        m_busyInboundMsgs.erase(senderIp); // Delete the entry for the sender because it is empty
      }
      
      this->ScheduleNewMsg(previouslyBusyMsg);
    }
    else
    {
      NS_LOG_ERROR("HomaRecvScheduler (" << this << ") detected a "
                   "Busy sender (" << senderIp << ") which doesn't "
                   "have a pending message");
        
      m_busyInboundMsgs.erase(senderIp); // Delete the entry for the sender because it is empty
    }
  } 
}
    
void HomaRecvScheduler::ForwardUp(Ptr<HomaInboundMsg> inboundMsg)
{
  NS_LOG_FUNCTION (this << inboundMsg);
    
  m_homa->ForwardUp (inboundMsg->GetReassembledMsg(), 
                     inboundMsg->GetIpv4Header (),
                     inboundMsg->GetSrcPort(),
                     inboundMsg->GetDstPort(),
                     inboundMsg->GetTxMsgId(),
                     inboundMsg->GetIpv4Interface());
        
  m_homa->SendDown(inboundMsg->GenerateGrantOrAck(m_homa->GetNumUnschedPrioBands(), 
                                                  HomaHeader::Flags_t::ACK),
                   inboundMsg->GetDstAddress (),
                   inboundMsg->GetSrcAddress ());
    
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
        
      Simulator::Cancel (currentMsg->GetRtxEvent ());
      m_activeInboundMsgs.erase(m_activeInboundMsgs.begin()+i);
      msgRemoved = true;
      break;
    }
  }
    
  if (!msgRemoved)
  {
    NS_LOG_ERROR("ERROR: HomaInboundMsg (" << inboundMsg <<
                 ") couldn't be found inside the active messages list!");
  }
}
    
void HomaRecvScheduler::RemoveMsgFromBusyMsgsList(Ptr<HomaInboundMsg> inboundMsg)
{
  NS_LOG_FUNCTION (this << inboundMsg);
    
  uint32_t senderIp = inboundMsg->GetSrcAddress().Get();
    
  bool msgRemoved = false;
  Ptr<HomaInboundMsg> currentMsg;
  auto busyMsgsOfSender = m_busyInboundMsgs.find(senderIp);
  if (busyMsgsOfSender != m_busyInboundMsgs.end())
  {
    for (std::size_t i = 0; i < busyMsgsOfSender->second.size(); ++i) 
    {
      currentMsg = busyMsgsOfSender->second[i];
      if (currentMsg->GetSrcAddress() == inboundMsg->GetSrcAddress() &&
          currentMsg->GetDstAddress() == inboundMsg->GetDstAddress() &&
          currentMsg->GetSrcPort() == inboundMsg->GetSrcPort() &&
          currentMsg->GetDstPort() == inboundMsg->GetDstPort() &&
          currentMsg->GetTxMsgId() == inboundMsg->GetTxMsgId())
      {
        NS_LOG_DEBUG("Erasing HomaInboundMsg (" << inboundMsg << 
                     ") from the busy messages list of HomaRecvScheduler (" << 
                     this << ").");
        busyMsgsOfSender->second.erase(busyMsgsOfSender->second.begin()+i);
        if (busyMsgsOfSender->second.size() == 0)
        {
          m_busyInboundMsgs.erase(senderIp); // Delete the entry for the sender because it is empty
        }
        msgRemoved = true;
        break;
      }
    }
  }
    
  if (!msgRemoved)
  {
    NS_LOG_ERROR("ERROR: HomaInboundMsg (" << inboundMsg <<
                 ") couldn't be found inside the busy messages list!");
  }
}
    
void HomaRecvScheduler::BusyReceivedForMsg(Ipv4Address senderAddress)
{
  NS_LOG_FUNCTION (this << senderAddress);
    
  std::vector<Ptr<HomaInboundMsg>> msgsToMarkBusy;
  for (std::size_t i = 0; i < m_activeInboundMsgs.size(); ++i) 
  {
    if (m_activeInboundMsgs[i]->GetSrcAddress() == senderAddress)
    {
      msgsToMarkBusy.push_back(m_activeInboundMsgs[i]);
      m_activeInboundMsgs.erase(m_activeInboundMsgs.begin()+i);
    } 
  }
   
  if (msgsToMarkBusy.size() > 0)
  {
    uint32_t senderIP = senderAddress.Get();
    auto busySenderMsgs = m_busyInboundMsgs.find(senderIP);
    if (busySenderMsgs != m_busyInboundMsgs.end())
    {
      // Sender was already marked busy before, append the active messages detected above
      busySenderMsgs->second.insert (busySenderMsgs->second.end(), 
                                     msgsToMarkBusy.begin(), msgsToMarkBusy.end());
    }
    else
    {
      m_busyInboundMsgs[senderIP] = msgsToMarkBusy;
    }
  }
}

/*
 * This method is called everytime a packet (DATA or BUSY) is received because
 * both types of incoming packets may cause the ordering of active messages list
 * which implies that we might need to send out a Grant.
 * The method loops over all the pending messages in the list of active messages
 * and checks whether they are grantable. If yes, an appropriate grant packet is 
 * generated and send down the networking stack. Although the method loops over
 * all the messages, ideally at most one message should be granted because other
 * messages should already be granted when they received packets for themselves.
 */
void HomaRecvScheduler::SendAppropriateGrants()
{
  NS_LOG_FUNCTION (this);
    
  std::list<Ipv4Address> grantedSenders; // Same sender can't be granted for multiple msgs at the same time
  uint8_t grantingPrio = m_homa->GetNumUnschedPrioBands(); // Scheduled priorities start here
  Ptr<HomaInboundMsg> currentMsg;
  for (std::size_t i = 0; i < m_activeInboundMsgs.size(); ++i) 
  {
    if (grantingPrio >= m_homa->GetNumTotalPrioBands()) 
      break;
    
    currentMsg = m_activeInboundMsgs[i];
    Ipv4Address senderAddress = currentMsg->GetSrcAddress ();
    if (std::find(grantedSenders.begin(),grantedSenders.end(),senderAddress) == grantedSenders.end())
    {
      if (!currentMsg->IsFullyGranted ()) // TODO: Fully granted active messages don't get reGranted?
      {
        if (currentMsg->IsGrantable ())
        {
          m_homa->SendDown(currentMsg->GenerateGrantOrAck(grantingPrio, 
                                                          HomaHeader::Flags_t::GRANT),
                           currentMsg->GetDstAddress (),
                           senderAddress);
        }
        grantingPrio++;
      }
    }
  }
}
    
void HomaRecvScheduler::ExpireRtxTimeout(Ptr<HomaInboundMsg> inboundMsg,
                                         uint16_t maxRsndPktOffset)
{
  NS_LOG_FUNCTION (this << inboundMsg << maxRsndPktOffset);
    
  // TODO: We need a cap for the max number of rtxTimeouts
    
  if (inboundMsg->IsFullyReceived ())
    return;
    
  // Create a dummy Homa Header for msg lookup
  HomaHeader homaHeader;
  homaHeader.SetSrcPort (inboundMsg->GetSrcPort ());
  homaHeader.SetDstPort (inboundMsg->GetDstPort ());
  homaHeader.SetTxMsgId (inboundMsg->GetTxMsgId ());
  int activeMsgIdx = -1;
  if (this->GetInboundMsg(inboundMsg->GetIpv4Header (), homaHeader, 
                          inboundMsg, activeMsgIdx))
  {
    if (inboundMsg->GetNumRtxWithoutProgress () >= m_homa->GetMaxNumRtxPerMsg())
    {
      NS_LOG_WARN(" Rtx Limit has been reached for the inbound Msg (" 
                  << inboundMsg << ").");
      if (activeMsgIdx >= 0)
        this->RemoveMsgFromActiveMsgsList (inboundMsg);
      else
        this->RemoveMsgFromBusyMsgsList (inboundMsg);
      
      return;
    }
      
    if (activeMsgIdx >= 0)
    {
      // We send RESEND packets only to non-busy senders
      NS_LOG_LOGIC(Simulator::Now().GetNanoSeconds () << 
                   " Rtx Timer for an inbound Msg (" << inboundMsg << 
                   ") expired, which is active. RESEND packets will be sent");
        
      std::list<Ptr<Packet>> rsndPkts = inboundMsg->GenerateResends (maxRsndPktOffset);
      while (!rsndPkts.empty())
      {
        m_homa->SendDown(rsndPkts.front(),
                         inboundMsg->GetDstAddress (),
                         inboundMsg->GetSrcAddress ());
        rsndPkts.pop_front();
      }
    }
      
    // Rechedule the next retransmission event for this message
    inboundMsg-> SetRtxEvent (Simulator::Schedule (m_homa->GetInboundRtxTimeout(), 
                                                   &HomaRecvScheduler::ExpireRtxTimeout, this, 
                                                   inboundMsg, inboundMsg->GetMaxGrantedIdx ()));
      
    // Update the LastRtxGrntIdx value of this message for the next timeout event
    uint16_t maxGrantableIdx = inboundMsg->GetMaxGrantableIdx ();
    if (inboundMsg->GetLastRtxGrntIdx () < maxGrantableIdx)
    {
      inboundMsg->ResetNumRtxWithoutProgress ();
    }
    else
    {
      inboundMsg->IncrNumRtxWithoutProgress ();
    }
    inboundMsg->SetLastRtxGrntIdx (maxGrantableIdx);
      
  }
  else
  {
    NS_LOG_DEBUG(Simulator::Now().GetNanoSeconds () << 
                 " Rtx Timer for an inbound Msg (" << inboundMsg << 
                 ") expired, which doesn't exist any more.");
  }  
  return;
}
    
} // namespace ns3