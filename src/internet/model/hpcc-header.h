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

#ifndef HPCC_HEADER_H
#define HPCC_HEADER_H

#include "ns3/header.h"

namespace ns3 {
/**
 * \ingroup hpcc
 * \brief Packet header for HPCC Transport packets
 *
 * This class has fields corresponding to those in a HPCC header
 * as well as methods for serialization to and deserialization 
 * from a byte buffer.
 */
class HpccHeader : public Header 
{
public:
  /**
   * \brief Constructor
   *
   * Creates a null header
   */
  HpccHeader ();
  ~HpccHeader ();
  
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
  
  /**
   * \param port The source port for this HpccHeader
   */
  void SetSrcPort (uint16_t port);
  /**
   * \return The source port for this HpccHeader
   */
  uint16_t GetSrcPort (void) const;
  
  /**
   * \param port The destination port for this HpccHeader
   */
  void SetDstPort (uint16_t port);
  /**
   * \return the destination port for this HpccHeader
   */
  uint16_t GetDstPort (void) const;
  
  /**
   * \param txMsgId The TX message ID for this HpccHeader
   */
  void SetTxMsgId (uint16_t txMsgId);
  /**
   * \return The source port for this HpccHeader
   */
  uint16_t GetTxMsgId (void) const;
  
  /**
   * \brief Set flags of the header
   * \param flags the flags for this HpccHeader
   */
  void SetFlags (uint8_t flags);
  /**
   * \brief Get the flags
   * \return the flags for this HpccHeader
   */
  uint8_t GetFlags () const;
  
  /**
   * \param pktOffset The packet identifier for this HpccHeader in number of packets 
   */
  void SetPktOffset (uint16_t pktOffset);
  /**
   * \return The packet identifier for this HpccHeader in number of packets 
   */
  uint16_t GetPktOffset (void) const;
  
  /**
   * \param msgSizeBytes The message size for this HpccHeader in bytes
   */
  void SetMsgSize (uint32_t msgSizeBytes);
  /**
   * \return The message size for this HpccHeader in bytes
   */
  uint32_t GetMsgSize (void) const;
  
  /**
   * \param payloadSize The payload size for this HpccHeader in bytes
   */
  void SetPayloadSize (uint16_t payloadSize);
  /**
   * \return The payload size for this HpccHeader in bytes
   */
  uint16_t GetPayloadSize (void) const;
  
  /**
   * \brief Converts an integer into a human readable list of HPCC flags
   *
   * \param flags Bitfield of HPCC flags to convert to a readable string
   * \param delimiter String to insert between flags
   *
   * \return the generated string
   **/
  static std::string FlagsToString (uint8_t flags, const std::string& delimiter = "|");
  
  /**
   * \brief HPCC flag field values
   */
  typedef enum Flags_t
  {
    DATA = 1,     //!< DATA Packet (Sent by the sender)
    ACK = 2,      //!< ACK (Sent by the receiver)
    UNKNOWN4 = 4,  //!< 
    UNKNOWN8 = 8,  //!< 
    UNKNOWN16 = 16, //!< 
    UNKNOWN32 = 32, //!< 
    UNKNOWN64 = 64, //!< 
    BOGUS = 128   //!< Used only in unit tests.
  } Flags_t;
  
  static const uint8_t PROT_NUMBER = 197; //!< Protocol number of HPCC
  
private:
  uint16_t m_srcPort;     //!< Source port
  uint16_t m_dstPort;     //!< Destination port
  uint16_t m_txMsgId;     //!< ID generated by the sender
  uint8_t m_flags;        //!< Flags
  uint16_t m_pktOffset;   //!< Similar to seq number (in packets)
  uint32_t m_msgSizeBytes;//!< Size of the message in bytes
  uint16_t m_payloadSize; //!< Payload size in Bytes
};
} // namespace ns3
#endif /* HPCC_HEADER */