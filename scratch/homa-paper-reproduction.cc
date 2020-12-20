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

// The topology used in this simulation is provided in Homa paper [1] in detail.
//
// The topology consists of 144 hosts divided among 9 racks with a 2-level switching 
// fabric. Host links operate at 10Gbps and TOR-aggregation links operate at 40 Gbps.
//
// [1] Behnam Montazeri, Yilong Li, Mohammad Alizadeh, and John Ousterhout.  
//     2018. Homa: a receiver-driven low-latency transport protocol using  
//     network priorities. In Proceedings of the 2018 Conference of the ACM  
//     Special Interest Group on Data Communication (SIGCOMM '18). Association  
//     for Computing Machinery, New York, NY, USA, 221–235. 
//     DOI:https://doi.org/10.1145/3230543.3230564

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

NS_LOG_COMPONENT_DEFINE ("HomaPaperReproduction");

void
AppSendTo (Ptr<Socket> senderSocket, 
           Ptr<Packet> appMsg, 
           InetSocketAddress receiverAddr)
{
  NS_LOG_FUNCTION(Simulator::Now ().GetNanoSeconds () << 
                  "Sending an application message.");
    
  int sentBytes = senderSocket->SendTo (appMsg, 0, receiverAddr);
  NS_LOG_INFO(sentBytes << " Bytes sent to " << receiverAddr);
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

std::map<double,int> ReadMsgSizeDist (std::string msgSizeDistFileName, double &avgMsgSizePkts)
{
  std::ifstream msgSizeDistFile;
  msgSizeDistFile.open (msgSizeDistFileName);
  NS_LOG_FUNCTION("Reading Msg Size Distribution From: " << msgSizeDistFileName);
    
  std::string line;
  std::istringstream lineBuffer;
  
  getline (msgSizeDistFile, line);
  lineBuffer.str (line);
  lineBuffer >> avgMsgSizePkts;
    
  std::map<double,int> msgSizeCDF;
  double prob;
  int msgSizePkts;
  while(getline (msgSizeDistFile, line)) 
  {
    lineBuffer.clear ();
    lineBuffer.str (line);
    lineBuffer >> msgSizePkts;
    lineBuffer >> prob;
      
    msgSizeCDF[prob] = msgSizePkts;
  }
  msgSizeDistFile.close();
    
  return msgSizeCDF;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
    
//   Packet::EnablePrinting ();
  Time::SetResolution (Time::NS);
  LogComponentEnable ("HomaPaperReproduction", LOG_LEVEL_DEBUG);  
//   LogComponentEnable ("MsgGeneratorApp", LOG_LEVEL_ALL);  
//   LogComponentEnable ("HomaSocket", LOG_LEVEL_ALL);
  LogComponentEnable ("HomaL4Protocol", LOG_LEVEL_ERROR);
    
  std::string msgSizeDistFileName ("inputs/homa-paper-reproduction/DCTCP-MsgSizeDist.txt");
  std::string msgTracesFileName ("outputs/homa-paper-reproduction/MsgTraces.tr");
    
  int nHosts = 144;
  int nTors = 9;
  int nSpines = 4;
    
  double networkLoad = 0.5;
  
  /******** Create Nodes ********/
  NodeContainer hostNodes;
  hostNodes.Create (nHosts);
    
  NodeContainer torNodes;
  torNodes.Create (nTors);
    
  NodeContainer spineNodes;
  spineNodes.Create (nSpines);
    
  /******** Create Channels ********/
  PointToPointHelper hostLinks;
  hostLinks.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  hostLinks.SetChannelAttribute ("Delay", StringValue ("250ns"));
  hostLinks.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
    
  PointToPointHelper aggregationLinks;
  aggregationLinks.SetDeviceAttribute ("DataRate", StringValue ("40Gbps"));
  aggregationLinks.SetChannelAttribute ("Delay", StringValue ("250ns"));
  aggregationLinks.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
    
  /******** Create NetDevices ********/
  NetDeviceContainer hostTorDevices[nHosts];
  for (int i = 0; i < nHosts; i++)
  {
    hostTorDevices[i] = hostLinks.Install (hostNodes.Get(i), 
                                           torNodes.Get(i/(nHosts/nTors)));
  }
    
  NetDeviceContainer torSpineDevices[nTors*nSpines];
  for (int i = 0; i < nTors; i++)
  {
    for (int j = 0; j < nSpines; j++)
    {
      torSpineDevices[i*nSpines+j] = aggregationLinks.Install (torNodes.Get(i), 
                                                               spineNodes.Get(j));
    }
  }
    
  /******** Install Internet Stack ********/
    
  /* Enable multi-path routing */
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_RANDOM));
    
  /* Set default BDP value in packets */
  Config::SetDefault("ns3::HomaL4Protocol::RttPackets", UintegerValue(7));
    
  /* Set default number of priority bands in the network */
  uint8_t numTotalPrioBands = 8;
  uint8_t numUnschedPrioBands = 2;
  Config::SetDefault("ns3::HomaL4Protocol::NumTotalPrioBands", 
                     UintegerValue(numTotalPrioBands));
  Config::SetDefault("ns3::HomaL4Protocol::NumUnschedPrioBands", 
                     UintegerValue(numUnschedPrioBands));
  
  InternetStackHelper stack;
  stack.InstallAll ();
    
  /* Link traffic control configuration for Homa compatibility */
  // TODO: The paper doesn't provide buffer sizes, so we set some large 
  //       value for rare overflows.
  TrafficControlHelper tchPfifoHoma;
  tchPfifoHoma.SetRootQueueDisc ("ns3::PfifoHomaQueueDisc", 
                             "MaxSize", StringValue("150p"),
                             "NumBands", UintegerValue(numTotalPrioBands));
  for (int i = 0; i < nHosts; i++)
  {
    tchPfifoHoma.Install (hostTorDevices[i].Get(1));
  }
  for (int i = 0; i < nTors*nSpines; i++)
  {
    tchPfifoHoma.Install (torSpineDevices[i]);
  }
   
  /* Set IP addresses of the nodes in the network */
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  std::vector<InetSocketAddress> clientAddresses;
    
  Ipv4InterfaceContainer hostTorIfs[nHosts];
  for (int i = 0; i < nHosts; i++)
  {
    hostTorIfs[i] = address.Assign (hostTorDevices[i]);
    clientAddresses.push_back(InetSocketAddress (hostTorIfs[i].GetAddress (0), 
                                                 1000+i));
    address.NewNetwork ();
  }
  
  Ipv4InterfaceContainer torSpineIfs[nTors*nSpines];
  for (int i = 0; i < nTors*nSpines; i++)
  {
    torSpineIfs[i] = address.Assign (torSpineDevices[i]);
    address.NewNetwork ();
  }
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    
  /******** Read the Workload Distribution From File ********/
  double avgMsgSizePkts;
  std::map<double,int> msgSizeCDF = ReadMsgSizeDist(msgSizeDistFileName, avgMsgSizePkts);
    
  NS_LOG_LOGIC ("The CDF of message sizes is given below: ");
  for (auto it = msgSizeCDF.begin(); it != msgSizeCDF.end(); it++)
  {
    NS_LOG_LOGIC (it->second << " : " << it->first);
  }
  NS_LOG_LOGIC("Average Message Size is: " << avgMsgSizePkts);
    
  /******** Create Message Generator Apps on End-hosts ********/
  HomaHeader homah;
  Ipv4Header ipv4h;
  uint32_t payloadSize = hostTorDevices[0].Get (0)->GetMtu() 
                         - homah.GetSerializedSize ()
                         - ipv4h.GetSerializedSize ();
    
  for (int i = 0; i < nHosts; i++)
  {
    Ptr<MsgGeneratorApp> app = CreateObject<MsgGeneratorApp>(hostTorIfs[i].GetAddress (0),
                                                             1000 + i, payloadSize);
    app->Install (hostNodes.Get (i), clientAddresses);
    app->SetWorkload (networkLoad, msgSizeCDF, avgMsgSizePkts);
      
    app->Start(Seconds (3.0));
    app->Stop(Seconds (3.001));
  }
      
  /* Set the message traces for the Homa clients*/
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> qStream;
  qStream = asciiTraceHelper.CreateFileStream (msgTracesFileName);
  Config::ConnectWithoutContext("/NodeList/*/$ns3::HomaL4Protocol/MsgBegin", 
                                MakeBoundCallback(&TraceMsgBegin, qStream));
  Config::ConnectWithoutContext("/NodeList/*/$ns3::HomaL4Protocol/MsgFinish", 
                                MakeBoundCallback(&TraceMsgFinish, qStream));

  /******** Run the Actual Simulation ********/
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}