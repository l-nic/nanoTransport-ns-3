/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2021 Stanford University
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

/*
 * The header implementation is not clearly described within the HPCC
 * (High Precision Congestion Control) paper. Therefore the implementation
 * provided below makes some simplifying assumptions. For example, in a 
 * network where HPCC (thus INT) is used, a packet has the following
 * headers respectively:
 *
 * IP -> INT -> HPCC
 * This allows INT header to be easily accessed by regular NetDevice logic
 * without major changes to InternetStack of the simulator. It is simply
 * seen as payload for the InternetStack.
 */

#include <stdint.h>
#include <iostream>

#include "hpcc-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HpccHeader");
    
NS_OBJECT_ENSURE_REGISTERED (HpccHeader);

/* The magic values below are used only for debugging.
 * They can be used to easily detect memory corruption
 * problems so you can see the patterns in memory.
 */
HpccHeader::HpccHeader ()
  : m_srcPort (0xfffd),
    m_dstPort (0xfffd),
    m_txMsgId (0),
    m_flags (0),
    m_pktOffset (0),
    m_msgSizeBytes (0),
    m_payloadSize (0)
{
}

HpccHeader::~HpccHeader ()
{
}
    
TypeId 
HpccHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HpccHeader")
    .SetParent<Header> ()
    .SetGroupName ("Internet")
    .AddConstructor<HpccHeader> ()
  ;
  return tid;
}
TypeId 
HpccHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
    
void 
HpccHeader::Print (std::ostream &os) const
{
  os << "length: " << m_payloadSize + GetSerializedSize ()
     << " " << m_srcPort << " > " << m_dstPort
     << " txMsgId: " << m_txMsgId
     << " pktOffset: " << m_pktOffset
     << " msgSize: " << m_msgSizeBytes
     << " " << FlagsToString (m_flags)
  ;
}

uint32_t 
HpccHeader::GetSerializedSize (void) const
{
  return 15;
}
    
std::string
HpccHeader::FlagsToString (uint8_t flags, const std::string& delimiter)
{
  static const char* flagNames[8] = {
    "DATA",
    "ACK",
    "UNKNOWN4",
    "UNKNOWN8",
    "UNKNOWN16",
    "UNKNOWN32",
    "UNKNOWN64",
    "BOGUS"
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
HpccHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
   
  i.WriteHtonU16 (m_srcPort);
  i.WriteHtonU16 (m_dstPort);
  i.WriteHtonU16 (m_txMsgId);
  i.WriteU8 (m_flags);
  i.WriteHtonU16 (m_pktOffset);
  i.WriteHtonU32 (m_msgSizeBytes);
  i.WriteHtonU16 (m_payloadSize);
}
uint32_t
HpccHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_srcPort = i.ReadNtohU16 ();
  m_dstPort = i.ReadNtohU16 ();
  m_txMsgId = i.ReadNtohU16 ();
  m_flags = i.ReadU8 ();
  m_pktOffset = i.ReadNtohU16 ();
  m_msgSizeBytes = i.ReadNtohU32 ();
  m_payloadSize = i.ReadNtohU16 ();

  return GetSerializedSize ();
}
    
void 
HpccHeader::SetSrcPort (uint16_t port)
{
  m_srcPort = port;
}
uint16_t 
HpccHeader::GetSrcPort (void) const
{
  return m_srcPort;
}
    
void 
HpccHeader::SetDstPort (uint16_t port)
{
  m_dstPort = port;
}
uint16_t 
HpccHeader::GetDstPort (void) const
{
  return m_dstPort;
}
    
void 
HpccHeader::SetTxMsgId (uint16_t txMsgId)
{
  m_txMsgId = txMsgId;
}
uint16_t 
HpccHeader::GetTxMsgId (void) const
{
  return m_txMsgId;
}

void
HpccHeader::SetFlags (uint8_t flags)
{
  m_flags = flags;
}
uint8_t
HpccHeader::GetFlags (void) const
{
  return m_flags;
}
    
void 
HpccHeader::SetPktOffset (uint16_t pktOffset)
{
  m_pktOffset = pktOffset;
}
uint16_t 
HpccHeader::GetPktOffset (void) const
{
  return m_pktOffset;
}
    
void 
HpccHeader::SetMsgSize (uint32_t msgSizeBytes)
{
  m_msgSizeBytes = msgSizeBytes;
}
uint32_t 
HpccHeader::GetMsgSize (void) const
{
  return m_msgSizeBytes;
}
    
void 
HpccHeader::SetPayloadSize (uint16_t payloadSize)
{
  m_payloadSize = payloadSize;
}
uint16_t 
HpccHeader::GetPayloadSize (void) const
{
  return m_payloadSize;
}
    
} // namespace ns3