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

// Network topology
//
//       n0    n1  ...   nSenders
//       \     \   /     /
//        \___ Switch __/
//
//
// - Tcp flows from n_i to n_j
// - DropTail queues

// This simulation is to reproduce the simple incast scenario described in
// "The nanoPU: Making the Network Interface a First-Class Citizen to 
// Minimize RPC Tail Latency" by Ibanez et.al.

#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NanoPuSimpleIncast");

void
SendSingleNdpPacket (Ptr<NdpNanoPuArcht> ndpNanoPu, 
                     Ipv4Address dstIp, uint16_t srcPort)
{
  NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << 
              " Sending a single packet long message" <<
              " through NDP NanoPU Archt " << ndpNanoPu <<
              " from port " << srcPort);
  
  uint32_t payloadSize = 1088-37; 
  Ptr<Packet> msg;
  msg = Create<Packet> (payloadSize);
    
  NanoPuAppHeader appHdr;
  appHdr.SetHeaderType((uint16_t) NANOPU_APP_HEADER_TYPE);
  appHdr.SetRemoteIp(dstIp);
  appHdr.SetRemotePort(9090);
  appHdr.SetLocalPort(srcPort);
  appHdr.SetMsgLen(1);
  appHdr.SetPayloadSize((uint16_t) payloadSize);
  msg-> AddHeader (appHdr);
    
  ndpNanoPu->Send (msg);
}

// static void
// BytesInQueueTrace (Ptr<OutputStreamWrapper> stream, 
//                      Ptr<Queue<Packet>> queue, 
//                      uint32_t oldval, uint32_t newval)
// {
//   NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
//                " Queue size from " << oldval << " to " << newval <<
//                " ("<< queue << ")");
//   *stream->GetStream () << Simulator::Now ().GetNanoSeconds () 
//                         << "\t" << newval << std::endl;
// }

static void
BytesInQueueDiscTrace (Ptr<OutputStreamWrapper> stream, 
                       Ptr<QueueDisc> queue_disc, 
                       uint32_t oldval, uint32_t newval)
{
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
               " Queue Disc size from " << oldval << " to " << newval <<
               " ("<< queue_disc << ")");
  *stream->GetStream () << Simulator::Now ().GetNanoSeconds () 
                        << "\t" << newval << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
    
  bool disablePacketTrimming = false;
  bool enablePcap = false;
    
  cmd.AddValue ("disablePacketTrimming", 
                "Boolean to determine packet trimming in the network", 
                disablePacketTrimming);
  cmd.AddValue ("enablePcap", 
                "Boolean to determine pcap tracing", enablePcap);
  cmd.Parse (argc, argv);
    
  Time::SetResolution (Time::NS);
//   LogComponentEnable ("NanoPuSimpleIncast", LOG_LEVEL_DEBUG);
//   LogComponentEnable ("NanoPuArcht", LOG_LEVEL_ALL);
//   LogComponentEnable ("NdpNanoPuArcht", LOG_LEVEL_ALL);
//   LogComponentEnable ("PfifoNdpQueueDisc", LOG_LEVEL_LOGIC);
//   LogComponentEnable ("Queue", LOG_LEVEL_INFO);
    
  if(!disablePacketTrimming){
    NS_LOG_DEBUG("Packet trimming enabled");
  }
    
  if(enablePcap){
    NS_LOG_INFO("Packet traces will be generated");
  }
  
  uint16_t numSenders = 80;
  uint64_t delay = 750; // In nanoseconds
  
  /* Defining the star topology */
    
  NodeContainer theSwitch;
  theSwitch.Create (1);
    
  NodeContainer switch2senders[numSenders];
  for(uint16_t i = 0 ; i < numSenders ; i++){
    switch2senders[i].Add (theSwitch.Get (0));
    switch2senders[i].Create (1);
  }
    
  NodeContainer switch2receiver;
  switch2receiver.Add (theSwitch.Get (0));
  switch2receiver.Create (1);
    
  /* Creating the channels */
    
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", 
                                   StringValue ("200Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", 
                                    TimeValue (NanoSeconds (delay)));
  pointToPoint.SetQueue ("ns3::DropTailQueue", 
                           "MaxSize", StringValue ("1p"));
    
  NetDeviceContainer senderDeviceContainers[numSenders];
  for(uint16_t i = 0 ; i < numSenders ; i++){
    senderDeviceContainers[i] = pointToPoint.Install (switch2senders[i]);
  }
    
  NetDeviceContainer receiverDeviceContainer;
  receiverDeviceContainer = pointToPoint.Install (switch2receiver);
    
  /* Install the Internet stack so that devices use IP */
    
  InternetStackHelper stack;
  stack.InstallAll ();
    
  /* Bottleneck link traffic control configuration for NDP compatibility */
    
  TrafficControlHelper tchPfifo;
  if(disablePacketTrimming)
  {
    tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", 
                             "MaxSize", StringValue("73p"));
  }
  else
  {
    tchPfifo.SetRootQueueDisc ("ns3::PfifoNdpQueueDisc", 
                             "MaxSize", StringValue("73p"));
  }
  for(uint16_t i = 0 ; i < numSenders ; i++){
    tchPfifo.Install (senderDeviceContainers[i]);
  }
  QueueDiscContainer qdc = tchPfifo.Install (receiverDeviceContainer);
   
  /* Collect instantaneous queue occupancy */
    
//   Ptr<NetDevice> switchDevice = receiverDeviceContainer.Get (0);
//   Ptr<PointToPointNetDevice> ptpSwitchDevice = 
//       DynamicCast<PointToPointNetDevice> (switchDevice);
//   Ptr<Queue<Packet>> queue = ptpSwitchDevice->GetQueue ();
  Ptr< QueueDisc > queue_disc = qdc.Get (0);
    
  AsciiTraceHelper asciiTraceHelper; // for creating streams of traces

  std::string qStreamName = "ndp-nanopu-simple-incast";
  if(disablePacketTrimming)
    qStreamName.append("-without-trimming");
  qStreamName.append(".qlen");
      
  Ptr<OutputStreamWrapper> qStream;
  qStream = asciiTraceHelper.CreateFileStream (qStreamName);
//   queue->TraceConnectWithoutContext ("BytesInQueue", 
//                                      MakeBoundCallback (&BytesInQueueTrace, 
//                                                         qStream, queue));
  queue_disc->TraceConnectWithoutContext ("BytesInQueue", 
                                          MakeBoundCallback (&BytesInQueueDiscTrace, 
                                                             qStream, queue_disc));
    
  /* Assign IP addresses */
    
  Ipv4AddressHelper addressHelper;
  char ipAddress[11];
    
  Ipv4InterfaceContainer senderIfContainers[numSenders]; 
  for(uint16_t i = 0 ; i < numSenders ; i++){
    sprintf(ipAddress,"10.1.%d.0",i+1);
    addressHelper.SetBase (ipAddress, "255.255.255.0");
    senderIfContainers[i] = addressHelper.Assign (senderDeviceContainers[i]);
  }
  Ipv4InterfaceContainer receiverIfContainer;
  sprintf(ipAddress,"10.1.%d.0",numSenders+1);
  addressHelper.SetBase (ipAddress, "255.255.255.0");
  receiverIfContainer = addressHelper.Assign (receiverDeviceContainer);
    
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    
  /* Define optional parameters for capacity of reassembly and packetize modules*/
    
  Time timeoutInterval = Time("13us");
  uint16_t maxMessages = 100;
  NdpHeader ndph;
  uint16_t ndpHeaderSize = (uint16_t) ndph.GetSerializedSize ();
  uint16_t payloadSize = senderDeviceContainers[0].Get (1)->GetMtu () - 40 - ndpHeaderSize;
    
  /* Enable the NanoPU Archt on the end points*/
  
  std::vector<Ptr<NdpNanoPuArcht>> senderArchts;
//   senderArchts.reserve(numSenders);
  for(uint16_t i = 0 ; i < numSenders ; i++){
    senderArchts.push_back(CreateObject<NdpNanoPuArcht>(switch2senders[i].Get (1), 
                                                        senderDeviceContainers[i].Get (1),
                                                        timeoutInterval, 
                                                        maxMessages, 
                                                        payloadSize));
    NS_LOG_INFO("**** Sender architecture "<< i <<" is created.");
  }   
  Ptr<NdpNanoPuArcht> receiverArcht =  CreateObject<NdpNanoPuArcht>(
                                                 switch2receiver.Get (1), 
                                                 receiverDeviceContainer.Get (1),
                                                 timeoutInterval, 
                                                 maxMessages, 
                                                 payloadSize);
  NS_LOG_INFO("**** Receiver architecture is created.");
   
  /* Schedule senders to create incast */
    
  Ipv4Address dstIp = receiverIfContainer.GetAddress (1);

  for(uint16_t i = 0 ; i < numSenders ; i++){
    Simulator::Schedule (Seconds (0.0), 
                         &SendSingleNdpPacket, 
                         senderArchts[i], 
                         dstIp, i+1);
  }
  
  /* Generate output files */
    
  std::string pcapStreamName = "ndp-nanopu-simple-incast";
  if(disablePacketTrimming){
    pcapStreamName.append("-without-trimming");
  }
    
  if (enablePcap){
    pointToPoint.EnablePcapAll (pcapStreamName, false);
  }
    
  pcapStreamName.append(".tr");
  pointToPoint.EnableAsciiAll (asciiTraceHelper.CreateFileStream (pcapStreamName));
  
  /* Run the actual simulation */
    
  Simulator::Run ();
  Simulator::Destroy ();
  
//   return 0;
}