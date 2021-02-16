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

// Star topology with 5 hosts to test basic functionality of 
// HomaNanoPU Archt sender
//
//    point-to-point
// sender -------------- switch -------------- receiver 0
//                             \-------------- receiver 1
//                              \------------- receiver 2
//                               \------------ receiver 3
//

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HomaNanoPuSenderEval");

void TraceMsgBegin (Ptr<OutputStreamWrapper> stream,
                    Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr, 
                    uint16_t sport, uint16_t dport, int txMsgId)
{
  NS_LOG_DEBUG("+ " << Simulator::Now ().GetNanoSeconds ()
                << " " << msg->GetSize()
                << " " << saddr << ":" << sport 
                << " "  << daddr << ":" << dport 
                << " " << txMsgId);
    
  *stream->GetStream () << "+ " << Simulator::Now ().GetNanoSeconds () 
      << " " << msg->GetSize()
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << std::endl;
}

void TraceMsgFinish (Ptr<OutputStreamWrapper> stream,
                     Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr, 
                     uint16_t sport, uint16_t dport, int txMsgId)
{
  NS_LOG_DEBUG("- " << Simulator::Now ().GetNanoSeconds () 
                << " " << msg->GetSize()
                << " " << saddr << ":" << sport 
                << " "  << daddr << ":" << dport 
                << " " << txMsgId);
    
  *stream->GetStream () << "- " << Simulator::Now ().GetNanoSeconds () 
      << " " << msg->GetSize()
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << std::endl;
}

static void
BytesInArbiterQueueTrace (Ptr<OutputStreamWrapper> stream,
                          uint32_t oldval, uint32_t newval)
{
  NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () <<
               " Arbiter Queue size from " << oldval << " to " << newval << ".");
    
  *stream->GetStream () << Simulator::Now ().GetNanoSeconds () 
      << " " << newval << std::endl;
}

void TraceDataPktArrival (Ptr<OutputStreamWrapper> stream,
                          Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr, 
                          uint16_t sport, uint16_t dport, int txMsgId,
                          uint16_t pktOffset, uint8_t prio)
{
  NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () 
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << " " << pktOffset << " " << (uint16_t)prio);
    
  *stream->GetStream () << Simulator::Now ().GetNanoSeconds () 
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << " " << pktOffset << " " << (uint16_t)prio << std::endl;
}

void SendMsg (Ptr<HomaNanoPuArcht> homaNanoPu, Ipv4Address dstIp, 
              uint16_t dstPort, uint32_t msgSize, uint16_t payloadSize)
{
  NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () <<
               " Sending a message of "<< msgSize << 
               " Bytes through Homa NanoPU Archt (" <<
               homaNanoPu << ") to " << dstIp <<
               ". (Payload size: " << payloadSize << ")");
   
  Ptr<Packet> msg;
  msg = Create<Packet> (msgSize);
    
  NanoPuAppHeader appHdr;
  appHdr.SetHeaderType((uint16_t) NANOPU_APP_HEADER_TYPE);
  appHdr.SetRemoteIp(dstIp);
  appHdr.SetRemotePort(dstPort);
  appHdr.SetLocalPort(100);
  appHdr.SetMsgLen(msgSize / payloadSize + (msgSize % payloadSize != 0));
  appHdr.SetPayloadSize(msgSize);
  msg-> AddHeader (appHdr);
    
  homaNanoPu->Send (msg);
}

int
main (int argc, char *argv[])
{
  AsciiTraceHelper asciiTraceHelper;
    
  double startTime = 3.0; // Seconds
  double rtt = 6.0e-6; // Seconds
    
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  Packet::EnablePrinting ();
  LogComponentEnable ("HomaNanoPuSenderEval", LOG_LEVEL_DEBUG);
//   LogComponentEnable ("NanoPuArcht", LOG_LEVEL_ALL);
//   LogComponentEnable ("HomaNanoPuArcht", LOG_LEVEL_DEBUG);
//   LogComponentEnable ("PfifoHomaQueueDisc", LOG_LEVEL_ALL);
    
  std::string tracesFileName ("outputs/homa-nanopu-impl-eval/SenderEval");
  std::string qStreamName = tracesFileName + ".qlen";
  std::string msgTracesFileName = tracesFileName + "-MsgTraces.tr";
  std::string pktTracesFileName = tracesFileName + "-PktTraces.tr";

  /******** Create Nodes ********/
  NS_LOG_UNCOND("Creating Nodes...");
  uint16_t nHosts = 5; // 1 Sender + 4 Receivers
    
  NodeContainer endHosts;
  endHosts.Create (nHosts);
    
  NodeContainer theSwitch;
  theSwitch.Create (1);
    
  /******** Create Channels ********/
  NS_LOG_UNCOND("Configuring Channels...");
  PointToPointHelper hostLinks;
  hostLinks.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  hostLinks.SetChannelAttribute ("Delay", StringValue ("883ns"));
  hostLinks.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
    
  /******** Create NetDevices ********/
  NS_LOG_UNCOND("Creating NetDevices...");
  PointerValue ptr;
    
  NetDeviceContainer netDeviceContainers[nHosts];
  for (int i = 0; i < nHosts; i++)
  {
    netDeviceContainers[i] = hostLinks.Install (endHosts.Get(i), 
                                                theSwitch.Get(0));
    // The queue on the end hosts should not be 1 packet large
    netDeviceContainers[i].Get (0)->GetAttribute ("TxQueue", ptr);
    ptr.Get<Queue<Packet> > ()->SetAttribute ("MaxSize", StringValue ("500p"));
  }
    
  /******** Install Internet Stack ********/
  NS_LOG_UNCOND("Installing Internet Stack...");
  InternetStackHelper stack;
  stack.Install (endHosts);
    
  /* Enable multi-path routing */
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_RANDOM));
    
  stack.Install (theSwitch);
    
  /* Link traffic control configuration for Homa compatibility */
  uint8_t numTotalPrioBands = 4;
  uint8_t numUnschedPrioBands = 1;
    
  TrafficControlHelper tchPfifoHoma;
  tchPfifoHoma.SetRootQueueDisc ("ns3::PfifoHomaQueueDisc", 
                                 "MaxSize", StringValue("500p"),
                                 "NumBands", UintegerValue(numTotalPrioBands));
    
  /* Set IP addresses of the nodes in the network */
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
    
  Ipv4InterfaceContainer ifContainers[nHosts];
  for (int i = 0; i < nHosts; i++)
  {
    ifContainers[i] = address.Assign (netDeviceContainers[i]);
    address.NewNetwork ();
  }
    
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    
  /* Set default number of priority bands in the network */
  NS_LOG_UNCOND("Deploying NanoPU Architectures...");
  HomaHeader homah;
  Ipv4Header ipv4h;
  uint16_t payloadSize = netDeviceContainers[0].Get (0)->GetMtu () 
                         - ipv4h.GetSerializedSize () 
                         - homah.GetSerializedSize ();
  Config::SetDefault("ns3::HomaNanoPuArcht::PayloadSize", 
                     UintegerValue(payloadSize));
  Config::SetDefault("ns3::HomaNanoPuArcht::TimeoutInterval", 
                     TimeValue(MilliSeconds(10)));
  Config::SetDefault("ns3::HomaNanoPuArcht::MaxNTimeouts", 
                     UintegerValue(5));
  Config::SetDefault("ns3::HomaNanoPuArcht::MaxNMessages", 
                     UintegerValue(100));
  uint16_t initialCredit = 5;
  Config::SetDefault("ns3::HomaNanoPuArcht::InitialCredit", 
                     UintegerValue(initialCredit));
  Config::SetDefault("ns3::HomaNanoPuArcht::OptimizeMemory", 
                     BooleanValue(true));
  Config::SetDefault("ns3::HomaNanoPuArcht::EnableArbiterQueueing", 
                     BooleanValue(true));
  Config::SetDefault("ns3::HomaNanoPuArcht::NumTotalPrioBands", 
                     UintegerValue(numTotalPrioBands));
  Config::SetDefault("ns3::HomaNanoPuArcht::NumUnschedPrioBands", 
                     UintegerValue(numUnschedPrioBands));
  Config::SetDefault("ns3::HomaNanoPuArcht::OvercommitLevel", 
                     UintegerValue(numTotalPrioBands-numUnschedPrioBands));
    
  std::vector<Ptr<HomaNanoPuArcht>> nanoPuArchts;
  for(int i = 0 ; i < nHosts ; i++)
  {
    nanoPuArchts.push_back(CreateObject<HomaNanoPuArcht>());
    nanoPuArchts[i]->AggregateIntoDevice(netDeviceContainers[i].Get (0));
    NS_LOG_INFO("**** NanoPU architecture "<< i <<" is created.");
  }
    
  /* Set the message traces for the Homa clients*/
  Ptr<OutputStreamWrapper> msgStream;
  msgStream = asciiTraceHelper.CreateFileStream (msgTracesFileName);
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::HomaNanoPuArcht/MsgBegin", 
                                MakeBoundCallback(&TraceMsgBegin, msgStream));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::HomaNanoPuArcht/MsgFinish", 
                                MakeBoundCallback(&TraceMsgFinish, msgStream));
    
  Ptr<OutputStreamWrapper> pktStream;
  pktStream = asciiTraceHelper.CreateFileStream (pktTracesFileName);
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::HomaNanoPuArcht/DataPktDeparture", 
                                MakeBoundCallback(&TraceDataPktArrival, pktStream));
  
  Ptr<OutputStreamWrapper> qStream = asciiTraceHelper.CreateFileStream (qStreamName);
  nanoPuArchts[0]->TraceConnectWithoutContext ("BytesInArbiterQueue", 
                              MakeBoundCallback (&BytesInArbiterQueueTrace, qStream));
    
  /******** Schedule Messages ********/
    
  Simulator::Schedule (Seconds (startTime), &SendMsg, 
                       nanoPuArchts[0], nanoPuArchts[1]->GetLocalIp (), 
                       101, 10*initialCredit*payloadSize, payloadSize);
    
  Simulator::Schedule (Seconds (startTime + rtt), &SendMsg, 
                       nanoPuArchts[0], nanoPuArchts[2]->GetLocalIp (), 
                       102, 5*initialCredit*payloadSize, payloadSize);
    
  Simulator::Schedule (Seconds (startTime + 2*rtt), &SendMsg, 
                       nanoPuArchts[0], nanoPuArchts[3]->GetLocalIp (), 
                       103, 8*initialCredit*payloadSize, payloadSize);
    
  Simulator::Schedule (Seconds (startTime + 3*rtt), &SendMsg, 
                       nanoPuArchts[0], nanoPuArchts[4]->GetLocalIp (), 
                       104, initialCredit*payloadSize, payloadSize);
    
  /******** Run the Actual Simulation ********/
  NS_LOG_UNCOND("Running the Simulation...");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}