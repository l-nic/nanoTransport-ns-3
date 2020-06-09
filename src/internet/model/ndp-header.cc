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

#include "ndp-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NdpHeader");
    
NS_OBJECT_ENSURE_REGISTERED (NdpHeader);

/* The magic values below are used only for debugging.
 * They can be used to easily detect memory corruption
 * problems so you can see the patterns in memory.
 */
NdpHeader::NdpHeader ()
  : m_srcPort (0xfffd),
    m_dstPort (0xfffd),
    m_txMsgId (0),
    m_flags (0),
    m_msgLen (0),
    m_pktOffset (0),
    m_pullOffset (0),
    m_payloadSize (0)
{
}

NdpHeader::~NdpHeader ()
{
}
    
TypeId 
NdpHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NdpHeader")
    .SetParent<Header> ()
    .SetGroupName ("Internet")
    .AddConstructor<NdpHeader> ()
  ;
  return tid;
}
TypeId 
NdpHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
    
void 
NdpHeader::Print (std::ostream &os) const
{
  os << "length: " << m_payloadSize + GetSerializedSize ()
     << " " << m_srcPort << " > " << m_dstPort
     << " msgLen: " << m_msgLen
     << " pktOffset: " << m_pktOffset
     << " pullOffset: " << m_pullOffset
     << " " << FlagsToString (m_flags)
  ;
}

uint32_t 
NdpHeader::GetSerializedSize (void) const
{
  /* Note: In htsim implementation, control packets of NDP are 64 bytes
   *       That would require another 15 bytes to be appended to the packet
   */
  return 15; 
}
    
std::string
NdpHeader::FlagsToString (uint8_t flags, const std::string& delimiter)
{
  static const char* flagNames[8] = {
    "DATA",
    "ACK",
    "NACK",
    "PULL",
    "CHOP",
    "F1",
    "F2",
    "F3"
  };
  std::string flagsDescription = "";
  for (uint8_t i = 0; i < 8; ++i)
    {
      if (flags & (1 << i))
        {
          if (flagsDescription.length () > 0)
            {
              flagsDescription += delimiter;
            }
          flagsDescription.append (flagNames[i]);
        }
    }
  return flagsDescription;
}
    
void
NdpHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  i.WriteHtonU16 (m_srcPort);
  i.WriteHtonU16 (m_dstPort);
  i.WriteHtonU16 (m_txMsgId);
  i.WriteU8 (m_flags);
  i.WriteHtonU16 (m_msgLen);
  i.WriteHtonU16 (m_pktOffset);
  i.WriteHtonU16 (m_pullOffset);
  i.WriteHtonU16 (m_payloadSize);
}
uint32_t
NdpHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_srcPort = i.ReadNtohU16 ();
  m_dstPort = i.ReadNtohU16 ();
  m_txMsgId = i.ReadNtohU16 ();
  m_flags = i.ReadU8 ();
  m_msgLen = i.ReadNtohU16 ();
  m_pktOffset = i.ReadNtohU16 ();
  m_pullOffset = i.ReadNtohU16 ();
  m_payloadSize = i.ReadNtohU16 ();

  return GetSerializedSize ();
}
    
void 
NdpHeader::SetSrcPort (uint16_t port)
{
  m_srcPort = port;
}
uint16_t 
NdpHeader::GetSrcPort (void) const
{
  return m_srcPort;
}
    
void 
NdpHeader::SetDstPort (uint16_t port)
{
  m_dstPort = port;
}
uint16_t 
NdpHeader::GetDstPort (void) const
{
  return m_dstPort;
}
    
void 
NdpHeader::SetTxMsgId (uint16_t txMsgId)
{
  m_txMsgId = txMsgId;
}
uint16_t 
NdpHeader::GetTxMsgId (void) const
{
  return m_txMsgId;
}

void
NdpHeader::SetFlags (uint8_t flags)
{
  m_flags = flags;
}
uint8_t
NdpHeader::GetFlags (void) const
{
  return m_flags;
}
    
void 
NdpHeader::SetMsgLen (uint16_t msgLen)
{
  m_msgLen = msgLen;
}
uint16_t 
NdpHeader::GetMsgLen (void) const
{
  return m_msgLen;
}
    
void 
NdpHeader::SetPktOffset (uint16_t pktOffset)
{
  m_pktOffset = pktOffset;
}
uint16_t 
NdpHeader::GetPktOffset (void) const
{
  return m_pktOffset;
}
    
void 
NdpHeader::SetPullOffset (uint16_t pullOffset)
{
  m_pullOffset = pullOffset;
}
uint16_t 
NdpHeader::GetPullOffset (void) const
{
  return m_pullOffset;
}
    
void 
NdpHeader::SetPayloadSize (uint16_t payloadSize)
{
  m_payloadSize = payloadSize;
}
uint16_t 
NdpHeader::GetPayloadSize (void) const
{
  return m_payloadSize;
}
    
} // namespace ns3