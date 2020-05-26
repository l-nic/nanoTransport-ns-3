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
#include "ns3/packet.h"
#include "node.h"
#include "nanopu-archt.h"

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
    
/* Inherit from Socket class: Returns associated node */
Ptr<Node>
NanoPuArcht::GetNode (void) const
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
      NS_ASSERT_MSG (found, "Socket cannot be bound to a NetDevice not existing on the Node");
    }
  m_boundnetdevice = netdevice;
  return;
}

Ptr<NetDevice>
NanoPuArcht::GetBoundNetDevice ()
{
  NS_LOG_FUNCTION (this);
  return m_boundnetdevice;
}
    
} // namespace ns3