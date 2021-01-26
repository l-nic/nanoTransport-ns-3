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

#ifndef NANOPU_TRAFFIC_GENERATOR_H
#define NANOPU_TRAFFIC_GENERATOR_H

#include "ns3/application.h"
#include "ns3/nanopu-archt.h"
#include "ns3/packet.h"
#include "ns3/event-id.h"

namespace ns3 {
    
class RandomVariableStream;
    
/**
 * \ingroup applications 
 * \defgroup nanopu-traffic-generator NanoPuTrafficGenerator
 *
 * This traffic generator binds to NanoPU architectures and
 * generate messages according to a given workload (message
 * rate and message size) distribution.
 */
class NanoPuTrafficGenerator : public Application 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuTrafficGenerator (Ptr<NanoPuArcht> nanoPu,
                          Ipv4Address remoteIp, uint16_t remotePort=9999);

  virtual ~NanoPuTrafficGenerator();
  
  void SetMsgRate (double mean);
  
  /**
   * \brief Configure the Random Variables for message sizes in bytes.
   */
  void SetMsgSize (double min, double max);
  
  void SetLocalPort (uint16_t port);
  
  void SetMaxMsg (uint16_t maxMsg);
  
  void StartImmediately (void);
  
  void Start(Time start);
  
  void Stop(Time stop);
  
protected:
  virtual void DoDispose (void);
private:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop
  
  //helpers
  /**
   * \brief Cancel the pending event.
   */
  void CancelNextEvent ();

  /**
   * \brief Schedule the next message to send.
   */
  void ScheduleNextMessage ();
  
  /**
   * \brief Send a message
   */
  void SendMessage ();
  
  /**
   * \brief Receive a message from the NanoPU Archt
   */
  void ReceiveMessage (Ptr<Packet> msg);
  
  Ptr<NanoPuArcht>     m_nanoPu;        //!< Associated nanoPu Architecture
  uint16_t        m_localPort;          //!< Local port number
  uint16_t        m_remotePort;         //!< Remote port number
  Ipv4Address     m_remoteIp;           //!< Remote IP address to send messages
  bool            m_startImmediately;   //!< True if first msg is to be sent right away
  Ptr<ExponentialRandomVariable>  m_msgRate;//!< rng for rate of message generation in sec/msg
  Ptr<UniformRandomVariable>  m_msgSize;//!< rng for size of generated messages in pkts
  uint16_t        m_maxPayloadSize;     //!< Maximum size of packet payloads
  uint16_t        m_maxMsg;             //!< Limit number of messages sent sent
  uint16_t        m_numMsg;             //!< Total number of messages sent so far
  EventId         m_nextSendEvent;      //!< Event id of pending "send msg" event

};

} // namespace ns3
#endif