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
#include "ns3/homa-header.h"
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
                                                              (uint32_t) m_node->GetDevice (0)->GetMtu (), 
                                                              m_bdp);
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
                                  uint32_t mtu, uint16_t bdp)
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
  m_maxPayloadSize = mtu - homah.GetSerializedSize () - ipv4h.GetSerializedSize ();
    
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
    
bool HomaSendScheduler::GetNextMsgIdAndPacket (uint16_t &txMsgId, Ptr<Packet> &p)
{
  NS_LOG_FUNCTION (this);
  
  Ptr<HomaOutboundMsg> currentMsg;
  Ptr<HomaOutboundMsg> candidateMsg;
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
  
  if (msgSelected)
  {
    HomaHeader homaHeader;
    homaHeader.SetDstPort (candidateMsg->GetDstPort ());
    homaHeader.SetSrcPort (candidateMsg->GetSrcPort ());
    homaHeader.SetTxMsgId (txMsgId);
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
    ipTosTag.SetTos (candidateMsg->GetPrio (pktOffset)); 
    // This packet may already have a SocketIpTosTag (see HomaSocket)
    p->ReplacePacketTag (ipTosTag);
    
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
      
    /* 
     * Calculate the time it would take to serialize this packet and schedule
     * the next TX of this scheduler accordingly.
     */
    Ipv4Header iph;
    PppHeader pph;
    uint32_t headerSize = iph.GetSerializedSize () + pph.GetSerializedSize ();
    Time txTime = m_txRate.CalculateBytesTxTime (p->GetSize () + headerSize);
    m_txEvent = Simulator::Schedule (txTime, &HomaSendScheduler::TxPacket, this);
  }
  else
  {
    NS_LOG_LOGIC("HomaSendScheduler doesn't have any packet to send!");
  }
}
    
} // namespace ns3