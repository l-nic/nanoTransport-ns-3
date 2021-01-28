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

NS_LOG_COMPONENT_DEFINE ("HomaNanoPuSimpleTest");

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("NanoPuArcht", LOG_LEVEL_FUNCTION);
  LogComponentEnable ("HomaNanoPuArcht", LOG_LEVEL_ALL);
  LogComponentEnable ("NanoPuTrafficGenerator", LOG_LEVEL_ALL);
//   LogComponentEnable ("PfifoHomaQueueDisc", LOG_LEVEL_ALL);
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
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10us"));
  pointToPoint.SetQueue ("ns3::DropTailQueue", 
                         "MaxSize", StringValue ("1p"));

  NetDeviceContainer deviceContainers[numEndPoints];
  deviceContainers[0] = pointToPoint.Install (nodeContainers[0]);
    
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  deviceContainers[1] = pointToPoint.Install (nodeContainers[1]);
  
  // Enable multi-path routing
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_RANDOM)); 
      
  InternetStackHelper stack;
  stack.InstallAll ();
    
  // Bottleneck link traffic control configuration for NDP compatibility
  TrafficControlHelper tchPfifo;
  tchPfifo.SetRootQueueDisc ("ns3::PfifoHomaQueueDisc", 
                             "MaxSize", StringValue("9p"),
                             "NumBands", UintegerValue(8));
  for( uint16_t i = 0 ; i < numEndPoints ; i++){
    tchPfifo.Install (deviceContainers[i]);
  }
    
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaceContainers[numEndPoints]; 
  for( uint16_t i = 0 ; i < numEndPoints ; i++){
    address.NewNetwork ();
    interfaceContainers[i] = address.Assign (deviceContainers[i]);
  }
    
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  /* Define an optional parameter for capacity of reassembly and packetize modules*/
  HomaHeader homah;
  Ipv4Header ipv4h;
  uint16_t payloadSize = deviceContainers[0].Get (1)->GetMtu () 
                         - ipv4h.GetSerializedSize () 
                         - homah.GetSerializedSize ();
  Config::SetDefault("ns3::HomaNanoPuArcht::PayloadSize", 
                     UintegerValue(payloadSize));
  Config::SetDefault("ns3::HomaNanoPuArcht::TimeoutInterval", 
                     TimeValue(MilliSeconds(100)));
  Config::SetDefault("ns3::HomaNanoPuArcht::MaxNTimeouts", 
                     UintegerValue(5));
  Config::SetDefault("ns3::HomaNanoPuArcht::MaxNMessages", 
                     UintegerValue(100));
  Config::SetDefault("ns3::HomaNanoPuArcht::InitialCredit", 
                     UintegerValue(10));
  Config::SetDefault("ns3::HomaNanoPuArcht::OptimizeMemory", 
                     BooleanValue(true));
   
  Ptr<HomaNanoPuArcht> srcArcht =  CreateObject<HomaNanoPuArcht>();
  srcArcht->AggregateIntoDevice (deviceContainers[0].Get (1));
  Ptr<HomaNanoPuArcht> dstArcht =  CreateObject<HomaNanoPuArcht>();
  dstArcht->AggregateIntoDevice (deviceContainers[1].Get (1));
    
  /* Currently each nanopu is able to connect to a single application only.
   *
   * Also note that every application on the same nanoPu (if there are multiple)
   * will bind to the exact same RecvCallback. This means all the applications
   * will be notified when a msg for a single application is received.
   * Applications should process the NanoPuAppHeader first to make sure
   * the incoming msg belongs to them.
   * TODO: implement a msg dispatching logic so that nanoPu delivers
   *       each msg only to the owner of the msg.
   */
  Ipv4Address senderIp = interfaceContainers[0].GetAddress(1);
  Ipv4Address receiverIp = interfaceContainers[1].GetAddress(1);
    
  NanoPuTrafficGenerator sender = NanoPuTrafficGenerator(srcArcht, receiverIp, 222);
  sender.SetLocalPort(111);
  sender.SetMsgSize(1,1); // Deterministically set the message size
  sender.SetMaxMsg(1);
  sender.StartImmediately();
  sender.Start(Seconds (3.0));
  
  NanoPuTrafficGenerator receiver = NanoPuTrafficGenerator(dstArcht, senderIp, 111);
  receiver.SetLocalPort(222);
    
//   pointToPoint.EnablePcapAll ("tmp.pcap", true);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
