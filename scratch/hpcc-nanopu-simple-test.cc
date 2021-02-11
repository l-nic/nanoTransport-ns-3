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
#include <chrono>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HpccNanoPuSimpleTest");

static void
BytesInArbiterQueueTrace (Ipv4Address saddr, 
                          uint32_t oldval, uint32_t newval)
{
  NS_LOG_INFO ("**** " << Simulator::Now ().GetNanoSeconds () <<
               " Arbiter Queue size from " << oldval << " to " << newval <<
               " ("<< saddr << ") ****");
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  Packet::EnablePrinting ();
  LogComponentEnable ("HpccNanoPuSimpleTest", LOG_LEVEL_DEBUG);
  LogComponentEnable ("NanoPuArcht", LOG_LEVEL_WARN);
  LogComponentEnable ("HpccNanoPuArcht", LOG_LEVEL_WARN);
  LogComponentEnable ("NanoPuTrafficGenerator", LOG_LEVEL_DEBUG);
//   LogComponentEnableAll (LOG_LEVEL_ALL);
    
  HpccHeader hpcch;
  IntHeader inth;
  Ipv4Header ipv4h;
  uint16_t payloadSize = 1000;
  uint16_t mtuBytes = payloadSize + ipv4h.GetSerializedSize () 
                      + inth.GetMaxSerializedSize () + hpcch.GetSerializedSize ();
  NS_LOG_DEBUG("MaxPayloadSize for HpccNanoPuArcht: " << payloadSize <<
               " and MTU: " << mtuBytes);

  /******** Create Nodes ********/
  NodeContainer tor2Agg;
  tor2Agg.Create (2);
    
  NodeContainer agg2Core;
  agg2Core.Add (tor2Agg.Get (1));
  agg2Core.Create (1);
    
  NodeContainer core2Agg;
  core2Agg.Add (agg2Core.Get (1));
  core2Agg.Create (1);
    
  NodeContainer agg2Tor;
  agg2Tor.Add (core2Agg.Get (1));
  agg2Tor.Create (1);
    
  NodeContainer tor2Sender;
  tor2Sender.Add (tor2Agg.Get (0));
  tor2Sender.Create (1);
    
  NodeContainer tor2Receiver;
  tor2Receiver.Add (agg2Tor.Get (1));
  tor2Receiver.Create (1);
    
  /******** Create Channels ********/
  PointToPointHelper hostChannel;
  hostChannel.SetDeviceAttribute ("EnableInt", BooleanValue (true));
  hostChannel.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  hostChannel.SetChannelAttribute ("Delay", StringValue ("1us"));
  hostChannel.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("32MB"));
    
  PointToPointHelper coreChannel;
  coreChannel.SetDeviceAttribute ("EnableInt", BooleanValue (true));
  coreChannel.SetDeviceAttribute ("DataRate", StringValue ("400Gbps"));
  coreChannel.SetChannelAttribute ("Delay", StringValue ("1us"));
  coreChannel.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("32MB"));

  /******** Create NetDevices ********/
  NetDeviceContainer tor2AggDevices;
  tor2AggDevices = coreChannel.Install (tor2Agg);
  tor2AggDevices.Get(0)->SetMtu (mtuBytes);
  tor2AggDevices.Get(1)->SetMtu (mtuBytes);
    
  NetDeviceContainer agg2CoreDevices;
  agg2CoreDevices = coreChannel.Install (agg2Core);
  agg2CoreDevices.Get(0)->SetMtu (mtuBytes);
  agg2CoreDevices.Get(1)->SetMtu (mtuBytes);
    
  NetDeviceContainer core2AggDevices;
  core2AggDevices = coreChannel.Install (core2Agg);
  core2AggDevices.Get(0)->SetMtu (mtuBytes);
  core2AggDevices.Get(1)->SetMtu (mtuBytes);
    
  NetDeviceContainer agg2TorDevices;
  agg2TorDevices = coreChannel.Install (agg2Tor);
  agg2TorDevices.Get(0)->SetMtu (mtuBytes);
  agg2TorDevices.Get(1)->SetMtu (mtuBytes);
    
  NetDeviceContainer tor2SenderDevices;
  tor2SenderDevices = hostChannel.Install (tor2Sender);
  tor2SenderDevices.Get(0)->SetMtu (mtuBytes);
  tor2SenderDevices.Get(1)->SetMtu (mtuBytes);
    
  NetDeviceContainer tor2ReceiverDevices;
  tor2ReceiverDevices = hostChannel.Install (tor2Receiver);
  tor2ReceiverDevices.Get(0)->SetMtu (mtuBytes);
  tor2ReceiverDevices.Get(1)->SetMtu (mtuBytes);
  
  /******** Install Internet Stack ********/
    
  /* Enable multi-path routing */
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_PER_FLOW));
      
  InternetStackHelper stack;
  stack.InstallAll ();
    
  /* Enable and configure traffic control layers */
  TrafficControlHelper tchPfifoFast;
  tchPfifoFast.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", 
                                 "MaxSize", StringValue("1p"));
    
  tchPfifoFast.Install (tor2AggDevices);
  tchPfifoFast.Install (agg2CoreDevices);
  tchPfifoFast.Install (core2AggDevices);
  tchPfifoFast.Install (agg2TorDevices);
  tchPfifoFast.Install (tor2SenderDevices);
  tchPfifoFast.Install (tor2ReceiverDevices);
    
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer tor2SenderIf = address.Assign (tor2SenderDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer tor2ReceiverIf = address.Assign (tor2ReceiverDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer tor2AggIf = address.Assign (tor2AggDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer agg2CoreIf = address.Assign (agg2CoreDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer core2AggIf = address.Assign (core2AggDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer agg2TorIf = address.Assign (agg2TorDevices);
    
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Define an optional/default parameters for modules*/
  Config::SetDefault("ns3::HpccNanoPuArcht::PayloadSize", 
                     UintegerValue(payloadSize));
  Config::SetDefault("ns3::HpccNanoPuArcht::TimeoutInterval", 
                     TimeValue(MilliSeconds(5)));
  Config::SetDefault("ns3::HpccNanoPuArcht::MaxNTimeouts", 
                     UintegerValue(5));
  Config::SetDefault("ns3::HpccNanoPuArcht::MaxNMessages", 
                     UintegerValue(100));
  Config::SetDefault("ns3::HpccNanoPuArcht::InitialCredit", 
                     UintegerValue(142));
  Config::SetDefault("ns3::HpccNanoPuArcht::BaseRTT", 
                     DoubleValue(MicroSeconds (13).GetSeconds ()));
  Config::SetDefault("ns3::HpccNanoPuArcht::WinAI", 
                     UintegerValue(80));
  Config::SetDefault("ns3::HpccNanoPuArcht::UtilFactor", 
                     DoubleValue(0.99));
  Config::SetDefault("ns3::HpccNanoPuArcht::MaxStage", 
                     UintegerValue(0));
  Config::SetDefault("ns3::HpccNanoPuArcht::OptimizeMemory", 
                     BooleanValue(true));
  Config::SetDefault("ns3::HpccNanoPuArcht::EnableArbiterQueueing", 
                     BooleanValue(false));
   
  Ptr<HpccNanoPuArcht> srcArcht =  CreateObject<HpccNanoPuArcht>();
  srcArcht->AggregateIntoDevice(tor2SenderDevices.Get (1));
  Ptr<HpccNanoPuArcht> dstArcht =  CreateObject<HpccNanoPuArcht>();
  dstArcht->AggregateIntoDevice(tor2ReceiverDevices.Get (1));
    
  srcArcht->TraceConnectWithoutContext ("PacketsInArbiterQueue", 
                                        MakeBoundCallback (&BytesInArbiterQueueTrace, 
                                                           tor2SenderIf.GetAddress (1)));
    
  Ipv4Address senderIp = tor2SenderIf.GetAddress(1);
  Ipv4Address receiverIp = tor2ReceiverIf.GetAddress(1);
    
  NanoPuTrafficGenerator senderApp = NanoPuTrafficGenerator(srcArcht, receiverIp, 222);
  senderApp.SetLocalPort(111);
  senderApp.SetMsgSize(BITMAP_SIZE,BITMAP_SIZE); // Deterministically set the message size
  senderApp.SetMaxMsg(1);
  senderApp.StartImmediately();
  senderApp.Start(Seconds (3.0));
  
  NanoPuTrafficGenerator receiverApp = NanoPuTrafficGenerator(dstArcht, senderIp, 111);
  receiverApp.SetLocalPort(222);
    
// //   pointToPoint.EnablePcapAll ("tmp.pcap", true);

  auto start = std::chrono::high_resolution_clock::now();
    
  Simulator::Run ();
    
  auto stop = std::chrono::high_resolution_clock::now(); 
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  NS_LOG_DEBUG("*** Time taken by simulation: "
                << duration.count() << " microseconds ***");
    
  Simulator::Destroy ();
  return 0;
}
