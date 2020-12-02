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
#include "ns3/ipv4-route.h"

#include "homa-l4-protocol.h"
#include "ns3/homa-header.h"
#include "homa-socket-factory.h"
#include "homa-socket.h"
#include "ipv4-end-point-demux.h"
#include "ipv4-end-point.h"
#include "ipv4-l3-protocol.h"

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
}

HomaL4Protocol::~HomaL4Protocol ()
{
  NS_LOG_FUNCTION_NOARGS ();
}
    
void 
HomaL4Protocol::SetNode (Ptr<Node> node)
{
  m_node = node;
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
        }
    }
  
  // We set our down target to the IPv4 send function.
  
  if (ipv4 != 0 && m_downTarget.IsNull())
    {
      ipv4->Insert (this);
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
                                                              (uint32_t) m_node->GetDevice (0)->GetMtu (), 
                                                              m_bdp);
  outMsg->SetRoute (route); // This is mostly unnecessary
  m_sendScheduler->ScheduleNewMessage(outMsg);
}
    
void
HomaL4Protocol::SendDown (Ptr<Packet> message, 
                          Ipv4Address saddr, Ipv4Address daddr, 
                          Ptr<Ipv4Route> route)
{
  NS_LOG_FUNCTION (this << message << saddr << daddr << route);
    
  m_downTarget (message, saddr, daddr, PROT_NUMBER, route);
}
    
enum IpL4Protocol::RxStatus
HomaL4Protocol::Receive (Ptr<Packet> packet,
                        Ipv4Header const &header,
                        Ptr<Ipv4Interface> interface)
{
  NS_LOG_FUNCTION (this << packet << header);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " HomaL4Protocol received: " << packet->ToString ());
  
  HomaHeader homaHeader;
  packet->PeekHeader (homaHeader);

  NS_LOG_DEBUG ("Looking up dst " << header.GetDestination () << " port " << homaHeader.GetDstPort ()); 
  Ipv4EndPointDemux::EndPoints endPoints =
    m_endPoints->Lookup (header.GetDestination (), homaHeader.GetDstPort (),
                         header.GetSource (), homaHeader.GetSrcPort (), interface);
  if (endPoints.empty ())
    {
      NS_LOG_LOGIC ("RX_ENDPOINT_UNREACH");
      return IpL4Protocol::RX_ENDPOINT_UNREACH;
    }
    
  //  TODO: Implement the protocol logic here!

  packet->RemoveHeader(homaHeader);
  for (Ipv4EndPointDemux::EndPointsI endPoint = endPoints.begin ();
       endPoint != endPoints.end (); endPoint++)
    {
      (*endPoint)->ForwardUp (packet->Copy (), header, homaHeader.GetSrcPort (), 
                              interface);
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
    
HomaOutboundMsg::HomaOutboundMsg (Ptr<Packet> message, 
                                  Ipv4Address saddr, Ipv4Address daddr, 
                                  uint16_t sport, uint16_t dport, 
                                  uint32_t mtu, uint16_t bdp)
    : m_route(0),
      m_prio(0)
{
  NS_LOG_FUNCTION (this);
      
  m_saddr = saddr;
  m_daddr = daddr;
  m_sport = sport;
  m_dport = dport;
    
  m_msgSizeBytes = message->GetSize ();
  m_remainingBytes = m_msgSizeBytes;
    
  HomaHeader homah;
  Ipv4Header ipv4h;
  m_maxPayloadSize = mtu - homah.GetSerializedSize () - ipv4h.GetSerializedSize ();
    
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
  m_maxGrantedIdx = bdp;
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
    
uint8_t HomaOutboundMsg::GetPrio ()
{
  // TODO: Determine priority of unscheduled packet according to
  //       distribution of message sizes.
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
      NS_LOG_LOGIC("HomaOutboundMsg (" << this 
                   << ") will send packet " << i << " next.");
      pktOffset = i;
      p = m_packets[i];
      return true;
    }
    i++;
  }
  return false;
}
    
/******************************************************************************/
    
const uint8_t HomaSendScheduler::MAX_N_MSG = 255;

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
    
  m_txMsgIdFreeList.resize(MAX_N_MSG);
  std::iota(m_txMsgIdFreeList.begin(), m_txMsgIdFreeList.end(), 0);
}

HomaSendScheduler::~HomaSendScheduler ()
{
  NS_LOG_FUNCTION_NOARGS ();
}
    
void HomaSendScheduler::SetPacer ()
{
  NS_LOG_FUNCTION (this);
    
  Ptr<NetDevice> netDevice = m_homa->GetNode ()->GetDevice (0);
  PointToPointNetDevice* p2pNetDevice = dynamic_cast<PointToPointNetDevice*>(&(*(netDevice))); 
    
  m_txRate = p2pNetDevice->GetDataRate ();
  m_pacerLastTxTime = Simulator::Now () - m_txRate.CalculateBytesTxTime ((uint32_t) netDevice->GetMtu ());
}
    
bool 
HomaSendScheduler::ScheduleNewMessage (Ptr<HomaOutboundMsg> outMsg)
{
  NS_LOG_FUNCTION (this << outMsg);
    
  uint16_t txMsgId;
  if (m_txMsgIdFreeList.size() > 0)
  {
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
      this->TxPacket();
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
    
void
HomaSendScheduler::TxPacket ()
{
  NS_LOG_FUNCTION (this);
    
  uint16_t nextTxMsgID;
  Ptr<Packet> p;
  if (this->GetNextMsgIdAndPacket (nextTxMsgID, p))
  {
    NS_LOG_LOGIC("HomaSendScheduler will transmit a packet from msg " << nextTxMsgID);
      
    Ptr<HomaOutboundMsg> nextMsg = m_outboundMsgs[nextTxMsgID];
    Ipv4Address saddr = nextMsg->GetSrcAddress ();
    Ipv4Address daddr = nextMsg->GetDstAddress ();
    Ptr<Ipv4Route> route = nextMsg->GetRoute ();
    
    m_homa->SendDown(p, saddr, daddr, route);
  }
  else
  {
    NS_LOG_LOGIC("HomaSendScheduler doesn't have any packet to send!");
  }
}
    
bool HomaSendScheduler::GetNextMsgIdAndPacket (uint16_t &txMsgId, Ptr<Packet> &p)
{
  NS_LOG_FUNCTION (this);
  
  uint16_t candidateID;
  uint16_t pktOffset;
  Ptr<HomaOutboundMsg> candidateMsg;
  for (auto& it: m_outboundMsgs) {
    candidateID = it.first;
    candidateMsg = it.second; 
    if (candidateMsg->GetNextPacket(pktOffset, p));
      break;
    // TODO: Iterate over all pending outbound messages and
    //       find the one that has the smallest remainingBytes,
    //       granted but not transmitted packets, and a receiver
    //       that is not listed as busy
  }
  txMsgId = candidateID;
    
  HomaHeader homaHeader;
  homaHeader.SetDstPort (candidateMsg->GetDstPort ());
  homaHeader.SetSrcPort (candidateMsg->GetSrcPort ());
  homaHeader.SetTxMsgId (candidateID);
  homaHeader.SetFlags (HomaHeader::Flags_t::DATA); 
  homaHeader.SetMsgLen (candidateMsg->GetMsgSizePkts ());
  homaHeader.SetPktOffset (pktOffset);
  homaHeader.SetPayloadSize (p->GetSize ());

  p->AddHeader (homaHeader);
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () << 
               " HomaL4Protocol sending: " << p->ToString ());
    
  // NOTE: Use the following SocketIpTosTag append strategy when 
  //       sending packets out. This allows us to set the priority
  //       of the packets correctly for the PfifoHomaQueueDisc way of 
  //       priority queueing in the network.
  SocketIpTosTag ipTosTag;
  ipTosTag.SetTos (candidateMsg->GetPrio ()); // TODO: Resolve Priorities
  // This packet may already have a SocketIpTosTag (see HomaSocket)
  p->ReplacePacketTag (ipTosTag);
    
  return true;
}
    
} // namespace ns3