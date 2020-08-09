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

#include <stdint.h>
#include <iostream>

#include "nanopu-app-header.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NanoPuAppHeader");
    
NS_OBJECT_ENSURE_REGISTERED (NanoPuAppHeader);

/* The magic values below are used only for debugging.
 * They can be used to easily detect memory corruption
 * problems so you can see the patterns in memory.
 */
NanoPuAppHeader::NanoPuAppHeader ()
  : m_headerType (0x9999),
    m_dstIp (Ipv4Address ()),
    m_dstPort (0xfffd),
    m_msgLen (0),
    m_payloadSize (0)
{
}

NanoPuAppHeader::~NanoPuAppHeader ()
{
}
    
TypeId 
NanoPuAppHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuAppHeader")
    .SetParent<Header> ()
    .SetGroupName ("Network")
    .AddConstructor<NanoPuAppHeader> ()
  ;
  return tid;
}
TypeId 
NanoPuAppHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
    
void 
NanoPuAppHeader::Print (std::ostream &os) const
{
  os << "length: " << m_payloadSize + GetSerializedSize ()
     << "Dst: " << m_dstIp.Get() << ":" << m_dstPort
     << " msgLen: " << m_msgLen
     << " type: " << m_headerType
  ;
}

uint32_t 
NanoPuAppHeader::GetSerializedSize (void) const
{
  return 12; 
}
    
void
NanoPuAppHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  i.WriteHtonU16 (m_headerType);
  i.WriteHtonU32 (m_dstIp.Get());
  i.WriteHtonU16 (m_dstPort);
  i.WriteHtonU16 (m_msgLen);
  i.WriteHtonU16 (m_payloadSize);
}
uint32_t
NanoPuAppHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_headerType = i.ReadNtohU16 ();
  m_dstIp.Set(i.ReadNtohU32 ());
  m_dstPort = i.ReadNtohU16 ();
  m_msgLen = i.ReadNtohU16 ();
  m_payloadSize = i.ReadNtohU16 ();

  return GetSerializedSize ();
}

void 
NanoPuAppHeader::SetHeaderType (uint16_t type)
{
  m_headerType = type;
}

uint16_t 
NanoPuAppHeader::GetHeaderType (void) const
{
  return m_headerType;
}
    
void 
NanoPuAppHeader::SetDstIp (Ipv4Address dstIp)
{
  m_dstIp = dstIp;
}
Ipv4Address 
NanoPuAppHeader::GetDstIp (void) const
{
  return m_dstIp;
}
    
void 
NanoPuAppHeader::SetDstPort (uint16_t port)
{
  m_dstPort = port;
}
uint16_t 
NanoPuAppHeader::GetDstPort (void) const
{
  return m_dstPort;
}
      
void 
NanoPuAppHeader::SetMsgLen (uint16_t msgLen)
{
  m_msgLen = msgLen;
}
uint16_t 
NanoPuAppHeader::GetMsgLen (void) const
{
  return m_msgLen;
}
   
void 
NanoPuAppHeader::SetPayloadSize (uint16_t payloadSize)
{
  m_payloadSize = payloadSize;
}
uint16_t 
NanoPuAppHeader::GetPayloadSize (void) const
{
  return m_payloadSize;
}
    
} // namespace ns3