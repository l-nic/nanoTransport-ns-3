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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NanoPuSimpleTest");

void
SendSingleNdpPacket (NetDeviceContainer devices)
{
  std::string payload = "NDP Payload";
  uint32_t payloadSize = payload.size () + 1;
    
  uint8_t *buffer;
  buffer = new uint8_t [payloadSize];
  memcpy (buffer, payload.c_str (), payloadSize);
   
  Ptr<Packet> ndp_p;
  ndp_p = Create<Packet> (buffer, payloadSize);
    
  NdpHeader ndph;
  ndph.SetSrcPort (111);
  ndph.SetDstPort (222);
  // TODO: Setting flag to DATA will throw error if no 
  //       application is written yet
  ndph.SetFlags (NdpHeader::Flags_t::ACK);
  ndph.SetMsgLen (1);
  ndph.SetPayloadSize ((uint16_t) payloadSize);
  ndp_p-> AddHeader (ndph);
    
  Ipv4Header iph;
  Ipv4Address src_ip = Ipv4Address ("1.1.1.1");
  iph.SetSource (src_ip);
  Ipv4Address dst_ip = Ipv4Address ("2.2.2.2");
  iph.SetDestination (dst_ip);
  ndp_p-> AddHeader (iph);
    
  devices.Get (0)->Send (ndp_p, devices.Get (1)->GetBroadcast (), 0x0800);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("NanoPuArcht", LOG_LEVEL_ALL);
  LogComponentEnable ("NdpNanoPuArcht", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

//   UdpEchoServerHelper echoServer (9);

//   ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
//   serverApps.Start (Seconds (1.0));
//   serverApps.Stop (Seconds (10.0));
  
  /* Define an optional parameter for capacity of reassembly and packetize modules*/
  uint16_t maxMessages = 100;
  NdpNanoPuArcht nanoPu = NdpNanoPuArcht(nodes.Get (1), maxMessages);
  nanoPu.BindToNetDevice (devices.Get (1));
    
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
    
  Packet::EnablePrinting ();

//   UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
//   echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
//   echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
//   echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

//   ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
//   echoClient.SetFill(clientApps.Get(0), "This is the payload of the packet!");
//   clientApps.Start (Seconds (2.0));
//   clientApps.Stop (Seconds (10.0));
  
  Simulator::Schedule (Seconds (3.0), &SendSingleNdpPacket, devices);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}