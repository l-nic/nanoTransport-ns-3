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

// Simple sender-receiver topology to test basic functionality of NanoPU Archt
// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//

#include <iostream>
#include <stdlib.h>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NanoPuSimpleTest");

void
SendSingleNdpPacket (NdpNanoPuArcht ndpNanoPu, 
                     Ipv4Address dstIp)
{
  NS_LOG_UNCOND("Sending a single packet long message through NDP NanoPU Archt");
  
  uint32_t payloadSize = 1400; 
  Ptr<Packet> msg;
  msg = Create<Packet> (payloadSize);
    
//   std::string payload = "NDP Payload";
//   uint32_t payloadSize = payload.size () + 1; 
//   uint8_t *buffer;
//   buffer = new uint8_t [payloadSize];
//   memcpy (buffer, payload.c_str (), payloadSize);
//   Ptr<Packet> msg;
//   msg = Create<Packet> (buffer, payloadSize);
    
  NanoPuAppHeader appHdr;
  appHdr.SetHeaderType((uint16_t) NANOPU_APP_HEADER_TYPE);
  appHdr.SetRemoteIp(dstIp);
  appHdr.SetRemotePort(222);
  appHdr.SetLocalPort(111);
  appHdr.SetMsgLen(1);
  appHdr.SetPayloadSize((uint16_t) payloadSize);
  msg-> AddHeader (appHdr);
    
  ndpNanoPu.Send (msg);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::FS);
  LogComponentEnable ("NanoPuArcht", LOG_LEVEL_ALL);
  LogComponentEnable ("NdpNanoPuArcht", LOG_LEVEL_ALL);
//   LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_ALL);
//   LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_ALL);
//   LogComponentEnable ("Packet", LOG_LEVEL_ALL);
    LogComponentEnable ("PfifoNdpQueueDisc", LOG_LEVEL_ALL);
//   LogComponentEnableAll (LOG_LEVEL_ALL);
  Packet::EnablePrinting ();

  /* Create the topology */
  uint16_t numEndPoints = 2;
    
  NodeContainer theSwitch;
  theSwitch.Create (1);
    
  NodeContainer nodeContainers[numEndPoints];
  for( uint16_t i = 0 ; i < numEndPoints ; i++){
    nodeContainers[i].Add (theSwitch.Get (0));
    nodeContainers[i].Create (1);
  }

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10us"));

  NetDeviceContainer deviceContainers[numEndPoints];
  for( uint16_t i = 0 ; i < numEndPoints ; i++){
    deviceContainers[i] = pointToPoint.Install (nodeContainers[i]);
  }

  InternetStackHelper stack;
  stack.InstallAll ();
    
  // Bottleneck link traffic control configuration for NDP compatibility
  TrafficControlHelper tchPfifo;
  tchPfifo.SetRootQueueDisc ("ns3::PfifoNdpQueueDisc", "MaxSize", StringValue("10p"));
  for( uint16_t i = 0 ; i < numEndPoints ; i++){
    tchPfifo.Install (deviceContainers[i]);
  }

  Ipv4AddressHelper address;
  char ipAddress[8];

  Ipv4InterfaceContainer interfaceContainers[numEndPoints]; 
  for( uint16_t i = 0 ; i < numEndPoints ; i++){
    sprintf(ipAddress,"10.1.%d.0",i+1);
    address.SetBase (ipAddress, "255.255.255.0");
    interfaceContainers[i] = address.Assign (deviceContainers[i]);
  }
    
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  /* Define an optional parameter for capacity of reassembly and packetize modules*/
  Time timeoutInterval = Time("100us");
  uint16_t maxMessages = 100;
  NdpHeader ndph;
  uint16_t ndpHeaderSize = (uint16_t) ndph.GetSerializedSize ();
  uint16_t payloadSize = deviceContainers[0].Get (1)->GetMtu () - 40 - ndpHeaderSize;
   
  /* Enable the NanoPU Archt on the end points*/
//   std::vector<NdpNanoPuArcht> ndpNanoPus;
//   for (uint16_t i=0; i<numNodes; i++)
//   {
//     NS_LOG_UNCOND("Creating NDP NanoPU Archt " << i);
//     ndpNanoPus[i] = NdpNanoPuArcht(nodes.Get (i), devices.Get (i), 
//                                 timeoutInterval, maxMessages, payloadSize);
      
//     NS_LOG_UNCOND("Created NDP NanoPU Archt " << i);
//   }
   NdpNanoPuArcht srcArcht =  NdpNanoPuArcht(nodeContainers[0].Get (1), 
                                             deviceContainers[0].Get (1),
                                             timeoutInterval, maxMessages, payloadSize);
   NdpNanoPuArcht dstArcht =  NdpNanoPuArcht(nodeContainers[1].Get (1), 
                                             deviceContainers[1].Get (1),
                                             timeoutInterval, maxMessages, payloadSize);
    
  /*
   * In order for an application to bind to the nanoPu architecture:
  nanoPu.GetReassemblyBuffer ()->SetRecvCallback (MakeCallback (&MyApplication::HandleMsg, this, msg));
  
   * where MyApplication::HandleMsg is the method to handle each reassembled msg
   * The signature for the MyApplication::HandleMsg method should be:
  void MyApplication::HandleMsg (Ptr<NanoPuArchtReassemble> reassemblyBuffer, Ptr<Packet> msg)
  
   * Note that "msg" has a type of packet which will have only the 
   * NanoPuAppHeader and the payload (msg itself). The application 
   * should remove the header first and operate on the payload.
   
   * Also note that every application on the same nanoPu will
   * bind to the exact same RecvCallback. This means all the applications
   * will be notified when a msg for a single application is received.
   * Applications should process the NanoPuAppHeader first to make sure
   * the incoming msg belongs to them.
   * TODO: implement a msg dispatching logic so that nanoPu delivers
   *       each msg only to the owner of the msg.
  */
  
  Ipv4Address dstIp = interfaceContainers[1].GetAddress(1);

  Simulator::Schedule (Seconds (3.0), &SendSingleNdpPacket, 
                                      srcArcht, dstIp);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}