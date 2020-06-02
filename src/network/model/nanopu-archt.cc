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
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "node.h"
#include "nanopu-archt.h"
#include "ns3/ipv4-header.h"

#include "ns3/udp-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NanoPuArcht");

NS_OBJECT_ENSURE_REGISTERED (NanoPuArcht);

TypeId NanoPuArcht::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuArcht")
    .SetParent<Object> ()
    .SetGroupName("Network")
  ;
  return tid;
}

NanoPuArcht::NanoPuArcht (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this);
  
  m_node = node;
  m_boundnetdevice = 0;
}

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
    
bool
NanoPuArcht::Send (Ptr<Packet> p)
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
    
  /*
   * ASSUMPTION: NanoPU will work with point to point channels, so sending a broadcast
   *             packet on L2 is equivalent to sending a unicast packet.
   * TODO: There should be a clever way of resolving the destination MAC address of the 
   *       switch that is connected to the NanoPU architecture via the m_boundnetdevice
   */
  return m_boundnetdevice->Send (p, m_boundnetdevice->GetBroadcast (), 0x0800);
}
    
bool NanoPuArcht::EnterIngressPipe( Ptr<NetDevice> device, Ptr<const Packet> p, 
                                    uint16_t protocol, const Address &from)
{
  Ptr<Packet> cp = p->Copy ();
  NS_LOG_FUNCTION (this << cp);
  NS_LOG_DEBUG ("At time " <<  Simulator::Now ().GetSeconds () << 
               " NanoPU received a packet of size " << cp->GetSize ());
    
  cp->Print (std::cout);
  std::cout << std::endl;
    
  Ipv4Header iph;
  cp->RemoveHeader (iph);
  NS_LOG_DEBUG ("This is the IP header: " << iph);
  Ipv4Address src_ip4 = iph.GetSource ();
  iph.SetSource (iph.GetDestination ());
  iph.SetDestination (src_ip4);
    
  UdpHeader udph;
  cp->RemoveHeader (udph);
  NS_LOG_DEBUG ("This is the UDP header: " << udph);
  uint16_t src_port = udph.GetSourcePort ();
  udph.SetSourcePort (udph.GetDestinationPort ());
  udph.SetDestinationPort (src_port);
  
  uint8_t *buffer = new uint8_t[cp->GetSize ()];
  cp->CopyData(buffer, cp->GetSize ());
  std::string s = std::string(buffer, buffer+cp->GetSize());
  NS_LOG_DEBUG ("This is the payload: " << s);
//   std::cout <<"  Payload: " << s << std::endl;
  
  cp->AddHeader (udph);
  cp->AddHeader (iph);
    
  Send(cp);
    
  return true;
}
    
} // namespace ns3