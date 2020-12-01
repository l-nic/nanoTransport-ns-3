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

// Simple sender-receiver topology to test basic functionality of Official Homa
// Default Network Topology
//
//     10.0.1.0/24        10.0.0.0/24       10.0.2.0/24
// n0 -------------- s0 -------------- s1 -------------- n1
//    point-to-point    point-to-point    point-to-point
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

NS_LOG_COMPONENT_DEFINE ("OfficialHomaSimpleTest");

void
AppSendTo (Ptr<Socket> senderSocket, 
           Ptr<Packet> appMsg, 
           InetSocketAddress receiverAddr)
{
  NS_LOG_FUNCTION(Simulator::Now ().GetNanoSeconds () << 
                  "Sending an application message.");
    
  int sentBytes = senderSocket->SendTo (appMsg, 0, receiverAddr);
  NS_LOG_DEBUG(sentBytes << " Bytes sent to " << receiverAddr);
}

void
AppReceive (Ptr<Socket> receiverSocket)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << 
                   "Received an application message");
 
  Ptr<Packet> message;
  Address from;
  while ((message = receiverSocket->RecvFrom (from)))
  {
    NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () << 
                 " client received " << message->GetSize () << " bytes from " <<
                 InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                 InetSocketAddress::ConvertFrom (from).GetPort ());
  }
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
    
  Packet::EnablePrinting ();
  Time::SetResolution (Time::NS);
  LogComponentEnable ("HomaSocket", LOG_LEVEL_ALL);
  LogComponentEnable ("HomaL4Protocol", LOG_LEVEL_ALL);
  LogComponentEnable ("OfficialHomaSimpleTest", LOG_LEVEL_ALL);
//   LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_ALL);

  /******** Create Nodes ********/
  NodeContainer switches;
  switches.Create (2);
    
  NodeContainer sender2switch;
  sender2switch.Add (switches.Get (0));
  sender2switch.Create (1);
    
  NodeContainer receiver2switch;
  receiver2switch.Add (switches.Get (1));
  receiver2switch.Create (1);

  /******** Create Channels ********/
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1us"));

  /******** Create NetDevices ********/
  NetDeviceContainer switchDevices;
  switchDevices = pointToPoint.Install (switches);
    
  NetDeviceContainer senderDevices;
  senderDevices = pointToPoint.Install (sender2switch);
    
  NetDeviceContainer receiveDevices;
  receiveDevices = pointToPoint.Install (receiver2switch);
    
  /******** Install Internet Stack ********/
  InternetStackHelper stack;
  stack.InstallAll ();
    
  /* Bottleneck link traffic control configuration for Homa compatibility */
  TrafficControlHelper tchPfifo;
  tchPfifo.SetRootQueueDisc ("ns3::PfifoHomaQueueDisc", 
                             "MaxSize", StringValue("9p"),
                             "NumBands", UintegerValue(4));
  tchPfifo.Install (switchDevices);
  tchPfifo.Install (senderDevices);
  tchPfifo.Install (receiveDevices);

  /* Enable multi-path routing */
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_RANDOM));

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer switchIf = address.Assign (switchDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer senderIf = address.Assign (senderDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer receiverIf = address.Assign (receiveDevices);
    
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /******** Create and Bind Homa Sockets on End-hosts ********/
  Ptr<SocketFactory> senderSocketFactory = sender2switch.Get(1)->GetObject<HomaSocketFactory> ();
  Ptr<Socket> senderSocket = senderSocketFactory->CreateSocket ();
  InetSocketAddress senderAddr = InetSocketAddress (senderIf.GetAddress (1), 1010);
  senderSocket->Bind (senderAddr);
    
  Ptr<SocketFactory> receiverSocketFactory = receiver2switch.Get(1)->GetObject<HomaSocketFactory> ();
  Ptr<Socket> receiverSocket = receiverSocketFactory->CreateSocket ();
  InetSocketAddress receiverAddr = InetSocketAddress (receiverIf.GetAddress (1), 2020);
  receiverSocket->Bind (receiverAddr);
    
  /******** Create a Message and Schedule to be Sent ********/
  HomaHeader homah;
  Ipv4Header ipv4h;
    
  uint32_t payloadSize = senderDevices.Get (1)->GetMtu() 
                         - homah.GetSerializedSize ()
                         - ipv4h.GetSerializedSize ();
  Ptr<Packet> appMsg = Create<Packet> (payloadSize);
  
  Simulator::Schedule (Seconds (3.0), &AppSendTo, senderSocket, appMsg, receiverAddr);
  receiverSocket->SetRecvCallback (MakeCallback (&AppReceive));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}