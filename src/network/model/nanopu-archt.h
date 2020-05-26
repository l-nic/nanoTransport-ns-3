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
#ifndef NANOPU_ARCHT_H
#define NANOPU_ARCHT_H

#include "ns3/object.h"

namespace ns3 {
    
class Node;
class Packet;

/**
 * \ingroup network
 * \defgroup nanopu-archt NanoPU Architecture
 */

/**
 * \ingroup nanopu-archt
 *
 * \brief Architecture for devices to replace internet and transport layers
 *
 */
class NanoPuArcht : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArcht (Ptr<Node> node);
  virtual ~NanoPuArcht ();
  
  /**
   * \brief Return the node this architecture is associated with.
   * \returns the node
   */
  virtual Ptr<Node> GetNode (void);
  
//   /** 
//    * \brief Allocate a local endpoint for this architecture.
//    * \param address the address to try to allocate
//    * \returns 0 on success, -1 on failure.
//    */
//   virtual int Bind (const Address &address) = 0;

//   /** 
//    * \brief Allocate a local IPv4 endpoint for this architecture.
//    *
//    * \returns 0 on success, -1 on failure.
//    */
//   virtual int Bind () = 0;

//   /** 
//    * \brief Allocate a local IPv6 endpoint for this architecture.
//    *
//    * \returns 0 on success, -1 on failure.
//    */
//   virtual int Bind6 () = 0;
  
  /**
   * \brief Bind the architecture to specific device.
   *
   * This method corresponds to using setsockopt() SO_BINDTODEVICE
   * of real network or BSD sockets.   If set, this option will
   * force packets to leave the bound device regardless of the device that
   * IP routing would naturally choose.  In the receive direction, only
   * packets received from the bound interface will be delivered.
   *
   * This option has no particular relationship to binding sockets to
   * an address via Socket::Bind ().  It is possible to bind sockets to a 
   * specific IP address on the bound interface by calling both 
   * Socket::Bind (address) and Socket::BindToNetDevice (device), but it
   * is also possible to bind to mismatching device and address, even if
   * the socket can not receive any packets as a result.
   *
   * \param netdevice Pointer to NetDevice of desired interface
   * \returns nothing
   */
  virtual void BindToNetDevice (Ptr<NetDevice> netdevice);

  /**
   * \brief Returns architecture's bound NetDevice, if any.
   *
   * This method corresponds to using getsockopt() SO_BINDTODEVICE
   * of real network or BSD sockets.
   * 
   * 
   * \returns Pointer to interface.
   */
  Ptr<NetDevice> GetBoundNetDevice (); 
  
protected:

    Ptr<Node> m_node; //!< the node this architecture is located at.
    Ptr<NetDevice> m_boundnetdevice; //!< the device this architecture is bound to (might be null).
};
    
} // namespace ns3
#endif