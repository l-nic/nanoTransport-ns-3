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

#ifndef NANOPU_APP_HEADER_H
#define NANOPU_APP_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"

namespace ns3 {
/**
 * \ingroup nanopu-archt
 * \brief Application header for NanoPu application messages
 *
 * This class has fields corresponding to those in an application header
 * as well as methods for serialization to and deserialization from a byte buffer.
 */
class NanoPuAppHeader : public Header 
{
public:
  /**
   * \brief Constructor
   *
   * Creates a null header
   */
  NanoPuAppHeader ();
  ~NanoPuAppHeader ();
  
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
   * \param type The type of the header (Should be 0x9999)
   */
  void SetHeaderType (uint16_t type);
  /**
   * \return The type of the header (Should be 0x9999)
   */
  uint16_t GetHeaderType (void) const;
  
  /**
   * \param remoteIp The remote address for the message
   */
  void SetRemoteIp (Ipv4Address remoteIp);
  /**
   * \return The remote address for the message
   */
  Ipv4Address GetRemoteIp (void) const;
  
  /**
   * \param port The remote port for the message
   */
  void SetRemotePort (uint16_t port);
  /**
   * \return the remote port for the message
   */
  uint16_t GetRemotePort (void) const;
  
  /**
   * \param port The local port for the message
   */
  void SetLocalPort (uint16_t port);
  /**
   * \return the local port for the message
   */
  uint16_t GetLocalPort (void) const;
  
  /**
   * \param msgLen The message length for the message in packets
   */
  void SetMsgLen (uint16_t msgLen);
  /**
   * \return The message length for the message in packets
   */
  uint16_t GetMsgLen (void) const;
  
  /**
   * \param payloadSize The payload size for the message in bytes
   */
  void SetPayloadSize (uint16_t payloadSize);
  /**
   * \return The payload size for the message in bytes
   */
  uint16_t GetPayloadSize (void) const;
  
private:

  uint16_t m_headerType;   //!< Field to verify this header
  Ipv4Address m_remoteIp;  //!< Remote IP address
  uint16_t m_remotePort;   //!< Remote port
  uint16_t m_localPort;    //!< Local port
  uint16_t m_msgLen;       //!< Length of the message in packets
  uint16_t m_payloadSize;  //!< Payload size in bytes
  
};
} // namespace ns3
#endif /* NDP_HEADER */