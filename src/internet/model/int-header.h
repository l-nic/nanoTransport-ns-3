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

#ifndef INT_HEADER_H
#define INT_HEADER_H

#include "ns3/header.h"

namespace ns3 {
    
// TODO: The original INT implementation advertised by the HPCC uses only 64
//       bits to represent the whole hop information. Should compress the struct
//       below 3 times to be compatible.
typedef struct intHop_t {
    uint64_t time;
    uint32_t txBytes;
    uint32_t qlen;
    uint64_t bitRate;
}intHop_t;
    
/**
 * \ingroup int
 * \brief A brief INT header implementation
 *
 * This class has fields corresponding to those in an INT header
 * as well as methods for serialization to and deserialization from a byte buffer.
 */
class IntHeader : public Header 
{
public:
  
  /**
   * \brief Constructor
   *
   * Creates a null header
   */
  IntHeader ();
  ~IntHeader ();
  
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  
  uint32_t GetMaxSerializedSize (void) const;
  
  /**
   * \return the protocol number of the next header after INT
   */
  uint8_t GetProtocol (void) const;
  
  /**
   * \param the protocol number of the next header after INT
   */
  void SetProtocol (uint8_t protocol);

  /**
   * \return the number of hops travelled by the packet.
   */
  uint16_t GetNHops (void) const;
  
  /**
   * Add information from a hop into the INT header if there is space in the header
   *
   * \return whether the information was successfully pushed or not
   */
  bool PushHop (uint64_t time, uint32_t bytes, uint32_t qlen, uint64_t rate);
  
  /**
   * \return the INT hop information for hop hopNo
   */
  intHop_t PeekHopN (uint16_t hopNo);
  
  /**
   * \param payloadSize The payload size for the packet in bytes
   */
  void SetPayloadSize (uint16_t payloadSize);
  /**
   * \return The payload size for the packet in bytes
   */
  uint16_t GetPayloadSize (void) const;
  
  static const uint8_t PROT_NUMBER = 196; //!< Protocol number of INT
  static const uint16_t MAX_INT_HOPS = 6; //!< Max number hops this INT header can store data of
  
private:

  uint8_t m_protocol; //!< The protocol number of the next header after INT.
  uint16_t m_nHops;   //!< Number of hops that are currently allocated on the header
  intHop_t m_intHops[MAX_INT_HOPS]; //!< The set of all INT information from each hop
  
  uint16_t m_payloadSize; //!< Payload size in bytes
  
};
} // namespace ns3
#endif /* INT_HEADER */