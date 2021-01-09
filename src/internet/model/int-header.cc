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

#include "int-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("IntHeader");
    
NS_OBJECT_ENSURE_REGISTERED (IntHeader);


IntHeader::IntHeader ()
  : m_protocol (0),
    m_nHops (0)
{
}

IntHeader::~IntHeader ()
{
}
    
TypeId 
IntHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::IntHeader")
    .SetParent<Header> ()
    .SetGroupName ("Network")
    .AddConstructor<IntHeader> ()
  ;
  return tid;
}
TypeId 
IntHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
    
void 
IntHeader::Print (std::ostream &os) const
{   
  os << "nHops: " << m_nHops;
    
  for (uint16_t i=0; i < m_nHops; i++)
  {
    os << " [Hop " << i << " -"
       << " ts: " << m_intHops[i].time
       << " txBytes: " << m_intHops[i].txBytes
       << " qlen: " << m_intHops[i].qlen
       << " bitRate: " << m_intHops[i].bitRate
       << " ]";
  }
  os << " nextProt: " << (uint32_t)m_protocol;
}

uint32_t 
IntHeader::GetSerializedSize (void) const
{
  return m_nHops * sizeof(intHop_t) + 5; 
}
    
uint32_t 
IntHeader::GetMaxSerializedSize (void) const
{
  return MAX_INT_HOPS * sizeof(intHop_t) + 5; 
}
    
void
IntHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  i.WriteU8 (m_protocol);
  i.WriteHtonU16 (m_nHops);
  for (uint16_t j=0; j < m_nHops; j++)
  {
    i.WriteHtonU64 (m_intHops[j].time);
    i.WriteHtonU32 (m_intHops[j].txBytes);
    i.WriteHtonU32 (m_intHops[j].qlen);
    i.WriteHtonU64 (m_intHops[j].bitRate);
  }
  i.WriteHtonU16 (m_payloadSize);
}
uint32_t
IntHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  
  m_protocol = i.ReadU8();
  m_nHops = i.ReadNtohU16 ();
  for (uint16_t j=0; j < m_nHops; j++)
  {
    m_intHops[j].time = i.ReadNtohU64 ();
    m_intHops[j].txBytes = i.ReadNtohU32 ();
    m_intHops[j].qlen = i.ReadNtohU32 ();
    m_intHops[j].bitRate = i.ReadNtohU64 ();
  }
  m_payloadSize = i.ReadNtohU16 ();

  return GetSerializedSize ();
}

uint8_t 
IntHeader::GetProtocol (void) const
{
  return m_protocol;
}
    
void IntHeader::SetProtocol (uint8_t protocol)
{
  m_protocol = protocol;
}
      
uint16_t 
IntHeader::GetNHops (void) const
{
  return m_nHops;
}
      
bool 
IntHeader::PushHop (uint64_t time, uint32_t bytes, uint32_t qlen, uint64_t rate)
{
  NS_LOG_FUNCTION(this);
    
  if (m_nHops < MAX_INT_HOPS)
  {
    m_intHops[m_nHops].time = time;
    m_intHops[m_nHops].txBytes = bytes;
    m_intHops[m_nHops].qlen = qlen;
    m_intHops[m_nHops].bitRate = rate;
    
    m_nHops++;
    return true;
  }
  return false;
}
    
intHop_t 
IntHeader::PeekHopN (uint16_t hopNo)
{
  NS_LOG_FUNCTION(this);
    
  intHop_t hopInfo;
  if (hopNo < m_nHops)
  {
    hopInfo.time = m_intHops[hopNo].time;
    hopInfo.txBytes = m_intHops[hopNo].txBytes;
    hopInfo.qlen = m_intHops[hopNo].qlen;
    hopInfo.bitRate = m_intHops[hopNo].bitRate;
  }
  else
  {
    hopInfo.time = 0;
    hopInfo.txBytes = 0;
    hopInfo.qlen = 0;
    hopInfo.bitRate = 0;
  }
  return hopInfo;
}
   
void 
IntHeader::SetPayloadSize (uint16_t payloadSize)
{
  m_payloadSize = payloadSize;
}
uint16_t 
IntHeader::GetPayloadSize (void) const
{
  return m_payloadSize;
}
    
} // namespace ns3