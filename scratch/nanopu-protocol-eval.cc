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

// Star topology with 10 senders to test performance of 
// various protocols under an incast scenario
//
//         point-to-point
// sender0 -------------- switch -------------- receiver
// sender1 --------------/
//  ...    -------------/
// sender9 ------------/
//

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <locale>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NanoPuProtoEval");

void TraceMsgBegin (Ptr<OutputStreamWrapper> stream,
                    Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr, 
                    uint16_t sport, uint16_t dport, int txMsgId)
{
  NS_LOG_INFO("+ " << Simulator::Now ().GetNanoSeconds ()
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
  NS_LOG_INFO("- " << Simulator::Now ().GetNanoSeconds () 
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
QueueOccupancyTrace (Ptr<OutputStreamWrapper> stream, 
                       uint32_t oldval, uint32_t newval)
{
  NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () <<
               " Queue Disc size from " << oldval << " to " << newval);
    
  *stream->GetStream () << Simulator::Now ().GetNanoSeconds ()
                        << " " << newval << std::endl;
}

void TraceDataPktArrival (Ptr<OutputStreamWrapper> stream,
                          Ptr<const Packet> p, Ipv4Address saddr, Ipv4Address daddr, 
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

void SendMsg (Ptr<NanoPuArcht> nanoPuArcht, Ipv4Address dstIp, 
              uint16_t dstPort, uint32_t msgSize, uint16_t payloadSize)
{
  NS_LOG_LOGIC(Simulator::Now ().GetNanoSeconds () << " Sending a " << 
               msgSize << " Bytes message through NanoPU Archt (" << 
               nanoPuArcht << ") to " << dstIp << ". (Payload size: " << 
               payloadSize << ")");
   
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
    
  nanoPuArcht->Send (msg);
}

int
main (int argc, char *argv[])
{
  AsciiTraceHelper asciiTraceHelper;
    
  /******** Set default variables ********/
  double startTime = 1.0; // Seconds
  int nSenders = 10;
  std::string bandwidth = "200Gbps";
  std::string delay = "50ns";
  uint32_t mtu = 1024; // Bytes
  std::string protocol = "ndp";
  double rto = 4.0; // Microseconds
  uint16_t initialCredit = 7;
  int buffSize = 80; // Packts
  uint8_t nTotalPrioBands = 4;
  uint8_t nUnschedPrioBands = 1;
    
  CommandLine cmd (__FILE__);
  cmd.AddValue ("nSenders", "The number of senders involved in the incast", nSenders);
  cmd.AddValue ("bw", "The link bandwidth accross the network", bandwidth);
  cmd.AddValue ("delay", "The link delay accross the network", delay);
  cmd.AddValue ("mtu", "The MTU accross the network", mtu);
  cmd.AddValue ("protocol", "The transport protocol to evaluate", protocol);
  cmd.AddValue ("initialCredit", "Number of packets initially sent as a burst", initialCredit);
  cmd.AddValue ("rto", "The retransmission timeout interval in microseconds", rto);
  cmd.AddValue ("bufferSize", "The bottleneck buffer size in packets", buffSize);
  cmd.AddValue ("nTotalPrioBands", "Number of priority bands (for Homa variants)", nTotalPrioBands);
  cmd.AddValue ("nUnschedPrioBands", 
                "Number of unscheduled priority bands (for Homa variants)", nUnschedPrioBands);
  cmd.Parse (argc, argv);
    
  std::string bufferSize (std::to_string(buffSize)+"p");
    
  std::locale loc;
  protocol[0] = std::toupper(protocol[0],loc);
    
  std::string tracesFileName ("outputs/nanopu-protocol-eval/");
  tracesFileName += protocol + "-Buff" + bufferSize;
  std::string qStreamName = tracesFileName + ".qlen";
  std::string msgTrName = tracesFileName + "-MsgTraces.tr";
  std::string pktTrName = tracesFileName + "-PktTraces.tr";
    
  Ptr<OutputStreamWrapper> qStream = asciiTraceHelper.CreateFileStream (qStreamName);
  Ptr<OutputStreamWrapper> msgStream = asciiTraceHelper.CreateFileStream (msgTrName);
  Ptr<OutputStreamWrapper> pktStream = asciiTraceHelper.CreateFileStream (pktTrName);
    
  protocol += "NanoPuArcht";
    
  Time::SetResolution (Time::NS);
  Packet::EnablePrinting ();
  LogComponentEnable ("NanoPuProtoEval", LOG_LEVEL_DEBUG);
  LogComponentEnable ("NanoPuArcht", LOG_LEVEL_WARN);
  LogComponentEnable (protocol.c_str(), LOG_LEVEL_WARN);
    
  protocol = std::string ("ns3::") + protocol;
    
  /******** Create Nodes ********/
  NS_LOG_DEBUG("Creating Nodes...");
  uint16_t nHosts = 1 + nSenders; // 1 Receiver + nSenders
    
  NodeContainer endHosts;
  endHosts.Create (nHosts);
    
  NodeContainer theSwitch;
  theSwitch.Create (1);
    
  /******** Create Channels ********/
  NS_LOG_DEBUG("Configuring Channels...");
  PointToPointHelper hostLinks;
  hostLinks.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  hostLinks.SetChannelAttribute ("Delay", StringValue (delay));
  hostLinks.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
    
  /******** Create NetDevices ********/
  NS_LOG_DEBUG("Creating NetDevices...");
  PointerValue ptr;
    
  NetDeviceContainer netDeviceContainers[nHosts];
  for (int i = 0; i < nHosts; i++)
  {
    netDeviceContainers[i] = hostLinks.Install (endHosts.Get(i), 
                                                theSwitch.Get(0));
    netDeviceContainers[i].Get(0)->SetMtu (mtu);
    netDeviceContainers[i].Get(1)->SetMtu (mtu);
      
    // The queue on the end hosts should not be 1 packet large
    netDeviceContainers[i].Get (0)->GetAttribute ("TxQueue", ptr);
    ptr.Get<Queue<Packet> > ()->SetAttribute ("MaxSize", StringValue (bufferSize));
  }
   
  /******** Install Internet Stack ********/
  NS_LOG_DEBUG("Installing Internet Stack...");
  InternetStackHelper stack;
  stack.Install (endHosts);
    
  /* Enable multi-path routing */
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_RANDOM));
    
  stack.Install (theSwitch);
    
  /* Link traffic control configuration for protocols */
  TrafficControlHelper tchPfifo;
  if (protocol.compare ("ns3::NdpNanoPuArcht") == 0)
  {
    tchPfifo.SetRootQueueDisc ("ns3::PfifoNdpQueueDisc", 
                               "MaxSize", StringValue(bufferSize));
  }
  else if (protocol.compare ("ns3::HomaNanoPuArcht") == 0)
  {
    tchPfifo.SetRootQueueDisc ("ns3::PfifoHomaQueueDisc", 
                               "MaxSize", StringValue(bufferSize),
                               "NumBands", UintegerValue(nTotalPrioBands));
  }
  else
  {
    NS_ABORT_MSG("This experiment is not designed for " << protocol);
  }
    
  QueueDiscContainer switchQdiscs[nHosts];
  for (int i = 0; i < nHosts; i++)
    switchQdiscs[i] = tchPfifo.Install (netDeviceContainers[i].Get(1));
    
  /* Trace bottleneck buffer occupancy */  
  switchQdiscs[0].Get(0)->TraceConnectWithoutContext ("BytesInQueue", 
                          MakeBoundCallback (&QueueOccupancyTrace, qStream));
    
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
    
  /* Configure NanoPuArcht objects */
  NS_LOG_DEBUG("Configuring NanoPU Architectures...");
    
  Config::SetDefault(protocol+"::TimeoutInterval", 
                     TimeValue(MicroSeconds(rto)));
  Config::SetDefault(protocol+"::MaxNTimeouts", 
                     UintegerValue(5));
  Config::SetDefault(protocol+"::MaxNMessages", 
                     UintegerValue(100));
  Config::SetDefault(protocol+"::InitialCredit", 
                     UintegerValue(initialCredit));
  Config::SetDefault(protocol+"::OptimizeMemory", 
                     BooleanValue(true));
  Config::SetDefault(protocol+"::EnableArbiterQueueing", 
                     BooleanValue(true));
   
  Ipv4Header ipv4h;
  uint16_t payloadSize = netDeviceContainers[0].Get (0)->GetMtu () 
                         - ipv4h.GetSerializedSize ();
  if (protocol.compare ("ns3::NdpNanoPuArcht") == 0)
  {
    NdpHeader ndph;
    payloadSize -= ndph.GetSerializedSize ();
  }
  else if (protocol.compare ("ns3::HomaNanoPuArcht") == 0)
  {
    HomaHeader homah;
    payloadSize -= homah.GetSerializedSize ();
    Config::SetDefault("ns3::HomaNanoPuArcht::NumTotalPrioBands", 
                       UintegerValue(nTotalPrioBands));
    Config::SetDefault("ns3::HomaNanoPuArcht::NumUnschedPrioBands", 
                       UintegerValue(nUnschedPrioBands));
    Config::SetDefault("ns3::HomaNanoPuArcht::OvercommitLevel", 
                       UintegerValue(nTotalPrioBands-nUnschedPrioBands));
  }
  else
  {
    NS_ABORT_MSG("This experiment is not designed for " << protocol);
  }
  
  Config::SetDefault(protocol+"::PayloadSize", 
                     UintegerValue(payloadSize));
    
  /* Create NanoPuArcht Objects */
  NS_LOG_DEBUG("Deploying NanoPU Architectures..."); 
    
  std::vector<Ptr<NanoPuArcht>> nanoPuArchts;
  for(int i = 0 ; i < nHosts ; i++)
  {
    Ptr<NanoPuArcht> nanoPuArcht;
    if (protocol.compare ("ns3::NdpNanoPuArcht") == 0)
    {
      Ptr<NdpNanoPuArcht> ndpNanoPuArcht = CreateObject<NdpNanoPuArcht>();
      ndpNanoPuArcht->AggregateIntoDevice(netDeviceContainers[i].Get (0));
      nanoPuArcht = ndpNanoPuArcht;
    }
    else if (protocol.compare ("ns3::HomaNanoPuArcht") == 0)
    {
      Ptr<HomaNanoPuArcht> homaNanoPuArcht = CreateObject<HomaNanoPuArcht>();
      homaNanoPuArcht->AggregateIntoDevice(netDeviceContainers[i].Get (0));
      nanoPuArcht = homaNanoPuArcht;
    }
    else
    {
      NS_ABORT_MSG("This experiment is not designed for " << protocol);
    }
      
    nanoPuArchts.push_back(nanoPuArcht);
    NS_LOG_INFO("**** NanoPU architecture "<< i <<" is deployed.");
  }
    
  /* Set the message traces for clients*/
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$"+protocol+"/MsgBegin", 
                                MakeBoundCallback(&TraceMsgBegin, msgStream));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$"+protocol+"/MsgFinish", 
                                MakeBoundCallback(&TraceMsgFinish, msgStream));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$"+protocol+"/DataPktArrival", 
                                MakeBoundCallback(&TraceDataPktArrival, pktStream));
    
  /******** Schedule Messages ********/
  for (uint16_t i = 1; i < nHosts; i++)
  {
    Simulator::Schedule (Seconds (startTime), &SendMsg, 
                         nanoPuArchts[i], nanoPuArchts[0]->GetLocalIp (), 
                         i, (10+(i-1)*2)*payloadSize, payloadSize);
  }
    
  /******** Run the Actual Simulation ********/
  NS_LOG_DEBUG("Running the Simulation...");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
    
}