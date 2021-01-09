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

// Simple sender-receiver topology to test basic functionality of HpccNanoPuArcht
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

NS_LOG_COMPONENT_DEFINE ("HpccNanoPuSimpleTest");

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("NanoPuArcht", LOG_LEVEL_ALL);
  LogComponentEnable ("HpccNanoPuArcht", LOG_LEVEL_ALL);
  LogComponentEnable ("NanoPuTrafficGenerator", LOG_LEVEL_ALL);
//   LogComponentEnableAll (LOG_LEVEL_ALL);
  Packet::EnablePrinting ();

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
  pointToPoint.SetDeviceAttribute ("EnableInt", BooleanValue (true));
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1us"));
  pointToPoint.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

  /******** Create NetDevices ********/
  NetDeviceContainer switchDevices;
  switchDevices = pointToPoint.Install (switches);
    
  NetDeviceContainer senderDevices;
  senderDevices = pointToPoint.Install (sender2switch);
    
  NetDeviceContainer receiveDevices;
  receiveDevices = pointToPoint.Install (receiver2switch);
  
  /******** Install Internet Stack ********/
    
  /* Enable multi-path routing */
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_PER_FLOW));
      
  InternetStackHelper stack;
  stack.InstallAll ();
    
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer switchIf = address.Assign (switchDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer senderIf = address.Assign (senderDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer receiverIf = address.Assign (receiveDevices);
    
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Define an optional/default parameters for modules*/
  Time timeoutInterval = MilliSeconds(10);
  uint16_t maxMessages = 100;
  HpccHeader hpcch;
  IntHeader inth;
  Ipv4Header ipv4h;
  uint16_t payloadSize = switchDevices.Get (1)->GetMtu () - ipv4h.GetSerializedSize () 
                         - inth.GetMaxSerializedSize () - hpcch.GetSerializedSize ();
  uint16_t initialCredit = 10; // in packets
  uint16_t maxTimeoutCnt = 5;
  Time baseRtt = MicroSeconds (13);
  uint32_t winAI = 80; // in Bytes    
  double utilFac = 0.95;
  uint16_t maxStage = 5;
   
  Ptr<HpccNanoPuArcht> srcArcht =  CreateObject<HpccNanoPuArcht>(sender2switch.Get (1), 
                                                                 senderDevices.Get (1),
                                                                 timeoutInterval, maxMessages, 
                                                                 payloadSize, initialCredit,
                                                                 maxTimeoutCnt, baseRtt.GetSeconds (), 
                                                                 winAI, utilFac, maxStage);
  Ptr<HpccNanoPuArcht> dstArcht =  CreateObject<HpccNanoPuArcht>(receiver2switch.Get (1), 
                                                                 receiveDevices.Get (1),
                                                                 timeoutInterval, maxMessages, 
                                                                 payloadSize, initialCredit,
                                                                 maxTimeoutCnt, baseRtt.GetSeconds (), 
                                                                 winAI, utilFac, maxStage);
    
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
  Ipv4Address senderIp = senderIf.GetAddress(1);
  Ipv4Address receiverIp = receiverIf.GetAddress(1);
    
  NanoPuTrafficGenerator senderApp = NanoPuTrafficGenerator(srcArcht, receiverIp, 222);
  senderApp.SetLocalPort(111);
  senderApp.SetMsgSize(1,1); // Deterministically set the message size
  senderApp.SetMaxMsg(1);
  senderApp.StartImmediately();
  senderApp.Start(Seconds (3.0));
  
  NanoPuTrafficGenerator receiverApp = NanoPuTrafficGenerator(dstArcht, senderIp, 111);
  receiverApp.SetLocalPort(222);
    
// //   pointToPoint.EnablePcapAll ("tmp.pcap", true);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
