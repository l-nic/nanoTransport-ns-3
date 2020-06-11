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
#include <unordered_map>
#include <tuple>
#include <list>
#include <numeric>

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "node.h"
#include "nanopu-archt.h"
#include "ns3/ipv4-header.h"

#include "ns3/udp-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NanoPuArcht");

NS_OBJECT_ENSURE_REGISTERED (NanoPuArcht);
    
/*
 * \brief Find the first set bit in the provided bitmap
 * 
 * \returns Index of first 1 from right to left, in binary representation of a bitmap
 */
uint16_t getFirstSetBitPos(bitmap_t n) { return (n!=0) ? log2(n & -n) : BITMAP_SIZE; };
    
/******************************************************************************/
    
TypeId NanoPuArchtEgressPipe::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtEgressPipe")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtEgressPipe::NanoPuArchtEgressPipe ()
{
  NS_LOG_FUNCTION (this);
}

NanoPuArchtEgressPipe::~NanoPuArchtEgressPipe ()
{
  NS_LOG_FUNCTION (this);
}
    
/******************************************************************************/
    
TypeId NanoPuArchtArbiter::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtArbiter")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtArbiter::NanoPuArchtArbiter ()
{
  NS_LOG_FUNCTION (this);
}

NanoPuArchtArbiter::~NanoPuArchtArbiter ()
{
  NS_LOG_FUNCTION (this);
}
    
void NanoPuArchtArbiter::SetEgressPipe (Ptr<NanoPuArchtEgressPipe> egressPipe)
{
  NS_LOG_FUNCTION (this);
    
  m_egressPipe = egressPipe;
}
    
/******************************************************************************/
    
TypeId NanoPuArchtPacketize::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtPacketize")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtPacketize::NanoPuArchtPacketize (Ptr<NanoPuArchtArbiter> arbiter)
{
  NS_LOG_FUNCTION (this);
    
  m_arbiter = arbiter;
}

NanoPuArchtPacketize::~NanoPuArchtPacketize ()
{
  NS_LOG_FUNCTION (this);
}
    
/******************************************************************************/
    
TypeId NanoPuArchtTimer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtTimer")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtTimer::NanoPuArchtTimer (Ptr<NanoPuArchtPacketize> packetize)
{
  NS_LOG_FUNCTION (this);
    
  m_packetize = packetize;
}

NanoPuArchtTimer::~NanoPuArchtTimer ()
{
  NS_LOG_FUNCTION (this);
}
    
/******************************************************************************/

TypeId NanoPuArchtReassemble::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArchtReassemble")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArchtReassemble::NanoPuArchtReassemble (uint16_t maxMessages)
{
  NS_LOG_FUNCTION (this);
    
  m_rxMsgIdFreeList.resize(maxMessages);
  std::iota(m_rxMsgIdFreeList.begin(), m_rxMsgIdFreeList.end(), 0);
}

NanoPuArchtReassemble::~NanoPuArchtReassemble ()
{
  NS_LOG_FUNCTION (this);
}
    
void 
NanoPuArchtReassemble::SetRecvCallback (Callback<void, 
                                                 Ptr<NanoPuArchtReassemble>, 
                                                 Ptr<Packet> > reassembledMsgCb)
{
  NS_LOG_FUNCTION (this << &reassembledMsgCb);
  m_reassembledMsgCb = reassembledMsgCb;
}
    
void NanoPuArchtReassemble::NotifyApplications (Ptr<Packet> msg)
{
  NS_LOG_FUNCTION (this << msg);
  if (!m_reassembledMsgCb.IsNull ())
    {
      m_reassembledMsgCb (this, msg);
    }
  else
    {
      NS_LOG_ERROR ("Error: NanoPU received a message but no application is looking for it" << this << msg);
    }
}

rxMsgInfoMeta_t 
NanoPuArchtReassemble::GetRxMsgInfo (Ipv4Address srcIp, uint16_t srcPort, 
                                     uint16_t txMsgId, uint16_t msgLen, 
                                     uint16_t pktOffset)
{
  NS_LOG_FUNCTION (this << srcIp << srcPort << txMsgId << msgLen << pktOffset);
    
  rxMsgInfoMeta_t rxMsgInfo;
  rxMsgInfo.isNewMsg = false;
  rxMsgInfo.isNewPkt = false;
  rxMsgInfo.success = false;
    
  rxMsgIdTableKey_t key (srcIp.Get (), srcPort, txMsgId);
  NS_LOG_LOGIC ("Processing GetRxMsgInfo extern call for: " << srcIp.Get () 
                                                     << "-" << srcPort
                                                     << "-" << txMsgId); 
  auto entry = m_rxMsgIdTable.find(key);
  if (entry != m_rxMsgIdTable.end())
  {
    rxMsgInfo.rxMsgId = entry->second;
    NS_LOG_LOGIC("Found rxMsgId: " << entry->second);
      
    // compute the beginning of the inflight window
    rxMsgInfo.ackNo = getFirstSetBitPos(~m_receivedBitmap.find (rxMsgInfo.rxMsgId)->second);
    if (rxMsgInfo.ackNo == BITMAP_SIZE)
    {
      NS_LOG_LOGIC("Msg " << rxMsgInfo.rxMsgId << "has already been fully received");
      rxMsgInfo.ackNo = msgLen;
    }
      
    rxMsgInfo.isNewPkt = (m_receivedBitmap.find (rxMsgInfo.rxMsgId)->second & (1<<pktOffset)) == 0;
    rxMsgInfo.success = true;
  }
  // try to allocate an rx_msg_id
  else if (m_rxMsgIdFreeList.size() > 0)
  {
    rxMsgInfo.rxMsgId = m_rxMsgIdFreeList.front ();
    NS_LOG_LOGIC("Allocating rxMsgId: " << rxMsgInfo.rxMsgId);
    m_rxMsgIdFreeList.pop_front ();
     
    m_rxMsgIdTable.insert({key,rxMsgInfo.rxMsgId});
    m_receivedBitmap.insert({rxMsgInfo.rxMsgId,0});
    std::map<uint16_t,Ptr<Packet>> buffer;
    m_buffers.insert({rxMsgInfo.rxMsgId,buffer});
    
    rxMsgInfo.ackNo = 0;
    rxMsgInfo.isNewMsg = true;
    rxMsgInfo.isNewPkt = true;
    rxMsgInfo.success = true;
  }
    
  return rxMsgInfo;
}
    
void 
NanoPuArchtReassemble::ProcessNewPacket (Ptr<Packet> pkt, reassembleMeta_t meta)
{
  NS_LOG_FUNCTION (this << pkt);
    
  NS_LOG_LOGIC ("Processing pkt "<< meta.pktOffset 
                << " for msg " << meta.rxMsgId);
    
  /* Record pkt in buffer*/
  auto buffer = m_buffers.find (meta.rxMsgId);
  NS_ASSERT (buffer != m_buffers.end ());
  buffer->second [meta.pktOffset] = pkt;
    
  /* Mark the packet as received*/
  // NOTE: received_bitmap must have 2 write ports: here and in getRxMsgInfo()
  m_receivedBitmap [meta.rxMsgId] |= (1<<meta.pktOffset);
    
  /* Check if all pkts have been received*/
  if (m_receivedBitmap [meta.rxMsgId] == static_cast<bitmap_t>((1<<meta.msgLen)-1))
  {
    NS_LOG_LOGIC ("All packets have been received for msg " << meta.rxMsgId);
      
    /* Push the reassembled msg to the applications*/
    
  }
}
    
/******************************************************************************/
    
TypeId NanoPuArcht::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArcht")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

/*
 * NanoPu Architecture requires a transport module.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for constructor
 */
NanoPuArcht::NanoPuArcht (Ptr<Node> node, uint16_t maxMessages)
{
  NS_LOG_FUNCTION (this);
    
  m_node = node;
  m_boundnetdevice = 0;
  m_maxMessages = maxMessages;
    
  m_reassemble = CreateObject<NanoPuArchtReassemble> (m_maxMessages);
  m_arbiter = CreateObject<NanoPuArchtArbiter> ();
  m_packetize = CreateObject<NanoPuArchtPacketize> (m_arbiter);
  m_timer = CreateObject<NanoPuArchtTimer> (m_packetize);
}

/*
 * NanoPu Architecture requires a transport module.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for destructor
 */
NanoPuArcht::~NanoPuArcht ()
{
  NS_LOG_FUNCTION (this);
}
    
/* Returns associated node */
Ptr<Node>
NanoPuArcht::GetNode (void)
{
  return m_node;
}

/*
 * NanoPu Architecture requires a transport module. The function below
 * is a reference implementation for future transport modules.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for the real implementation.
 */
void
NanoPuArcht::BindToNetDevice (Ptr<NetDevice> netdevice)
{
  NS_LOG_FUNCTION (this << netdevice);
    
  if (netdevice != 0)
    {
      bool found = false;
      for (uint32_t i = 0; i < GetNode ()->GetNDevices (); i++)
        {
          if (GetNode ()->GetDevice (i) == netdevice)
            {
              found = true;
              break;
            }
        }
      NS_ASSERT_MSG (found, "NanoPU cannot be bound to a NetDevice not existing on the Node");
    }
  m_boundnetdevice = netdevice;
  m_boundnetdevice->SetReceiveCallback (MakeCallback (&NanoPuArcht::EnterIngressPipe, this));
  m_mtu = m_boundnetdevice->GetMtu ();
  return;
}

Ptr<NetDevice>
NanoPuArcht::GetBoundNetDevice ()
{
  NS_LOG_FUNCTION (this);
  return m_boundnetdevice;
}
    
Ptr<NanoPuArchtReassemble> 
NanoPuArcht::GetReassemblyBuffer (void)
{
  NS_LOG_FUNCTION (this);
  return m_reassemble;
}
    
bool
NanoPuArcht::Send (Ptr<Packet> p, const Address &dest)
{
  NS_LOG_FUNCTION (this << p);
  NS_ASSERT_MSG (m_boundnetdevice != 0, "NanoPU doesn't have a NetDevice to send the packet to!"); 
  
  /*
  somewhere in the eggress pipe we will need to set the source ipv4 address of the packet 
  we can use the logic below:
  
  Ptr<Ipv4> ipv4proto = GetNode ()->GetObject<Ipv4> ();
  int32_t ifIndex = ipv4proto->GetInterfaceForDevice (device);
  Ipv4Address srcIP = ipv4proto->SourceAddressSelection (ifIndex, Ipv4Address dest);
  
  where dest is the dstIP provided by the AppHeader
  */

  return m_boundnetdevice->Send (p, dest, 0x0800);
}

/*
 * NanoPu Architecture requires a transport module. The function below
 * is a reference implementation for future transport modules.
 * see ../../internet/model/.*-nanopu-transport.{h/cc) for the real implementation.
 */
bool NanoPuArcht::EnterIngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                                    uint16_t protocol, const Address &from)
{
  NS_LOG_FUNCTION (this << p);
  NS_LOG_DEBUG ("At time " <<  Simulator::Now ().GetSeconds () << 
               " NanoPU received a packet of size " << p->GetSize ());
    
  return false;
}
    
} // namespace ns3