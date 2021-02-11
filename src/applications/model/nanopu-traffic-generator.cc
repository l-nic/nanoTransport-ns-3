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
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/nanopu-app-header.h"
#include "ns3/callback.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "nanopu-traffic-generator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NanoPuTrafficGenerator");

NS_OBJECT_ENSURE_REGISTERED (NanoPuTrafficGenerator);

TypeId
NanoPuTrafficGenerator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::NanoPuTrafficGenerator")
    .SetParent<Application> ()
    .SetGroupName("Applications")
//     .AddConstructor<NanoPuTrafficGenerator> ()
//     .AddAttribute ("LocalPort", "The local port number to bind to.",
//                    UintegerValue (9999),
//                    MakeUintegerAccessor (&NanoPuTrafficGenerator::m_localPort),
//                    MakeUintegerChecker<uint16_t> ())
//     .AddAttribute ("MsgRate", "A RandomVariableStream used to pick the message inter-departure times.",
//                    StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),
//                    MakePointerAccessor (&NanoPuTrafficGenerator::m_mesgRate),
//                    MakePointerChecker <RandomVariableStream>())
//     .AddAttribute ("MsgSize", "A RandomVariableStream used to pick the message sizes.",
//                    StringValue ("ns3::ConstantRandomVariable[Constant=15.0]"),
//                    MakePointerAccessor (&NanoPuTrafficGenerator::m_msgSize),
//                    MakePointerChecker <RandomVariableStream>())
//     .AddAttribute ("MaxMsg", 
//                    "The total number of messages to send. The value zero means "
//                    "that there is no limit.",
//                    UintegerValue (0),
//                    MakeUintegerAccessor (&OnOffApplication::m_maxBytes),
//                    MakeUintegerChecker<uint16_t> ()) 
  ;
  return tid;
}


NanoPuTrafficGenerator::NanoPuTrafficGenerator (Ptr<NanoPuArcht> nanoPu,
                                                Ipv4Address remoteIp, 
                                                uint16_t remotePort)
  : m_localPort (9999),
    m_startImmediately (false),
    m_maxMsg (100),
    m_numMsg (0)
{
  NS_LOG_FUNCTION (this);
        
  m_nanoPu = nanoPu;
  
  m_nanoPu->SetRecvCallback (MakeCallback (&NanoPuTrafficGenerator::ReceiveMessage, 
                                           this));
        
  m_remoteIp = remoteIp;
  m_remotePort = remotePort;
        
  m_msgRate = CreateObject<ExponentialRandomVariable> ();
  m_msgRate->SetAttribute ("Mean", DoubleValue (0.1));
  m_msgRate->SetAttribute ("Bound", DoubleValue (1));
        
  m_msgSize = CreateObject<UniformRandomVariable> ();
  m_msgSize->SetAttribute ("Min", DoubleValue (1.0));
  m_msgSize->SetAttribute ("Max", DoubleValue (20.0));
  
  m_maxPayloadSize = m_nanoPu->GetPayloadSize(); 
}

NanoPuTrafficGenerator::~NanoPuTrafficGenerator()
{
  NS_LOG_FUNCTION (this);
}

void NanoPuTrafficGenerator::SetMsgRate (double mean)
{
  NS_LOG_FUNCTION (this);
    
  m_msgRate->SetAttribute ("Mean", DoubleValue (1.0/mean));
  m_msgRate->SetAttribute ("Bound", DoubleValue (10.0/mean));
}
    
void NanoPuTrafficGenerator::SetMsgSize (double min, double max)
{
  NS_LOG_FUNCTION (this);
    
  m_msgSize->SetAttribute ("Min", DoubleValue (min));
  m_msgSize->SetAttribute ("Max", DoubleValue (max));
}
    
void NanoPuTrafficGenerator::SetLocalPort (uint16_t port)
{
  NS_LOG_FUNCTION (this);
    
  m_localPort = port;
}
    
void NanoPuTrafficGenerator::SetMaxMsg (uint16_t maxMsg)
{
  NS_LOG_FUNCTION (this);
    
  m_maxMsg = maxMsg;
}

void NanoPuTrafficGenerator::StartImmediately (void)
{
  NS_LOG_FUNCTION (this);
    
  m_startImmediately = true;
}
   
void NanoPuTrafficGenerator::Start (Time start)
{
  NS_LOG_FUNCTION (this);
    
  SetStartTime(start);
  DoInitialize();
}
    
void NanoPuTrafficGenerator::Stop (Time stop)
{
  NS_LOG_FUNCTION (this);
    
  SetStopTime(stop);
}
 
void
NanoPuTrafficGenerator::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  CancelNextEvent ();
  // chain up
  Application::DoDispose ();
}
    
void NanoPuTrafficGenerator::StartApplication ()
{
  NS_LOG_FUNCTION (this);
    
  if (m_startImmediately)
    SendMessage();
  else
    ScheduleNextMessage ();
}
    
void NanoPuTrafficGenerator::StopApplication ()
{
  NS_LOG_FUNCTION (this);
    
  CancelNextEvent();
}
    
void NanoPuTrafficGenerator::CancelNextEvent()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  if (!Simulator::IsExpired(m_nextSendEvent))
    Simulator::Cancel (m_nextSendEvent);
}
    
void NanoPuTrafficGenerator::ScheduleNextMessage ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  Time nextTime (Seconds (m_msgRate->GetValue ()));
  m_nextSendEvent = Simulator::Schedule (nextTime,
                                         &NanoPuTrafficGenerator::SendMessage, 
                                         this);
}
    
void NanoPuTrafficGenerator::SendMessage ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
  
  double msgSizePkts = m_msgSize->GetValue();
  uint32_t msgSizeBytes = (uint32_t) (msgSizePkts * (double) m_maxPayloadSize);
  uint16_t numPkts = (uint16_t) std::ceil(msgSizePkts); 
  Ptr<Packet> msg;
  msg = Create<Packet> (msgSizeBytes);   
//   std::string payload = "Custom Payload";
//   uint32_t msgSizeBytes = payload.size () + 1; 
//   uint8_t *buffer;
//   buffer = new uint8_t [msgSizeBytes];
//   memcpy (buffer, payload.c_str (), msgSizeBytes);
//   Ptr<Packet> msg;
//   msg = Create<Packet> (buffer, msgSizeBytes);
    
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
                " NanoPuTrafficGenerator (" << m_nanoPu->GetLocalIp () << 
                ") generates a message of size: "<< msgSizeBytes << 
                " Bytes " << numPkts << " packets.");
    
  NanoPuAppHeader appHdr;
  appHdr.SetHeaderType((uint16_t) NANOPU_APP_HEADER_TYPE);
  appHdr.SetRemoteIp(m_remoteIp);
  appHdr.SetRemotePort(m_remotePort);
  appHdr.SetLocalPort(m_localPort);
  appHdr.SetMsgLen(numPkts);
  appHdr.SetPayloadSize(msgSizeBytes);
  msg-> AddHeader (appHdr);
    
  if (m_nanoPu->Send (msg))
  {
    m_numMsg++;
  }
    
  if (m_maxMsg == 0 || m_numMsg < m_maxMsg)
  {
    ScheduleNextMessage ();
  }
    
}
    
void NanoPuTrafficGenerator::ReceiveMessage (Ptr<Packet> msg)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
               " NanoPuTrafficGenerator (" << m_nanoPu->GetLocalIp () << 
               ") received a message: " << msg->ToString ());
    
//   NanoPuAppHeader appHdr;
//   msg-> RemoveHeader (appHdr);
//   NS_LOG_DEBUG("\t\t\t message size: " << msg->GetSize ());
}
    
} // Namespace ns3
