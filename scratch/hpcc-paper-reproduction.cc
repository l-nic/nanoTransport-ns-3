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

// The topology used in this simulation is provided in HPCC paper [1] in detail.
//
// The topology consists of 320 hosts divided among 20 racks with a 2-level switching 
// fabric. Host links operate at 100Gbps and TOR-aggregation links operate at 400 Gbps.
//
// [1] Yuliang Li, Rui Miao, Hongqiang Harry Liu, Yan Zhuang, Fei Feng, 
//     Lingbo Tang, Zheng Cao, Ming Zhang, Frank Kelly, Mohammad Alizadeh, 
//     and Minlan Yu. 2019. HPCC: high precision congestion control. In 
//     Proceedings of the ACM Special Interest Group on Data Communication 
//     (SIGCOMM '19). Association for Computing Machinery, New York, NY, USA, 
//     44–58. DOI:https://doi-org.stanford.idm.oclc.org/10.1145/3341302.3342085

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>

#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HpccPaperReproduction");

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

void SendMsg (Ptr<HpccNanoPuArcht> hpccNanoPu, Ipv4Address dstIp, 
              uint16_t dstPort, uint32_t msgSize, uint16_t payloadSize)
{
  NS_LOG_DEBUG(Simulator::Now ().GetNanoSeconds () <<
               " Sending a message of "<< msgSize << 
               " Bytes through HPCC NanoPU Archt (" <<
               hpccNanoPu << ") to " << dstIp <<
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
    
  hpccNanoPu->Send (msg);
}

int
main (int argc, char *argv[])
{
  AsciiTraceHelper asciiTraceHelper;
  double duration = 0.25;
  double networkLoad = 0.5;
  uint32_t simIdx = 0;
  bool traceQueues = false;
  std::string workloadName ("FbHdp");
    
  
  HpccHeader hpcch;
  IntHeader inth;
  Ipv4Header ipv4h;
  uint16_t mtuBytes = 1000 + ipv4h.GetSerializedSize () 
                      + inth.GetMaxSerializedSize () + hpcch.GetSerializedSize ();
    
  CommandLine cmd (__FILE__);
  cmd.AddValue ("duration", "The duration of the simulation in seconds.", duration);
  cmd.AddValue ("load", "The network load to simulate the network at, ie 0.5 for 50%.", networkLoad);
  cmd.AddValue ("simIdx", "The index of the simulation used to identify parallel runs.", simIdx);
  cmd.AddValue ("traceQueues", "Whether to trace the queue lengths during the simulation.", traceQueues);
  cmd.Parse (argc, argv);
    
  SeedManager::SetRun (simIdx);
  Time::SetResolution (Time::NS);
//   Packet::EnableChecking ();
//   Packet::EnablePrinting ();
//   LogComponentEnable ("HpccPaperReproduction", LOG_LEVEL_DEBUG);  
//   LogComponentEnable ("NanoPuArcht", LOG_LEVEL_FUNCTION);
//   LogComponentEnable ("HpccNanoPuArcht", LOG_LEVEL_FUNCTION);
    
  std::string inputTraceFileName ("inputs/hpcc-paper-reproduction/");
  inputTraceFileName += workloadName + "Trace";
  inputTraceFileName += "L" + std::to_string((int)(networkLoad*100)) + "p";
  inputTraceFileName += "T" + std::to_string((int)(duration*1000)) + "ms.tr";
  std::string outputTracesFileName ("outputs/hpcc-paper-reproduction/FlowTraces");
  outputTracesFileName += workloadName;
  outputTracesFileName += "L" + std::to_string((int)(networkLoad*100)) + "p";
  outputTracesFileName += "T" + std::to_string((int)(duration*1000)) + "ms.tr";
    
  int nHosts = 320;
  int nTors = 20;
  int nAggSw = 20;
  int nCoreSw = 16;
  
  /******** Create Nodes ********/
  NS_LOG_UNCOND("Creating Nodes...");
  NodeContainer hostNodes;
  hostNodes.Create (nHosts);
    
  NodeContainer torNodes;
  torNodes.Create (nTors);
    
  NodeContainer aggNodes;
  aggNodes.Create (nAggSw);
    
  NodeContainer coreNodes;
  coreNodes.Create (nCoreSw);
    
  /******** Create Channels ********/
  NS_LOG_UNCOND("Configuring Channels...");
  PointToPointHelper hostLinks;
  hostLinks.SetDeviceAttribute ("EnableInt", BooleanValue (true));
  hostLinks.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  hostLinks.SetChannelAttribute ("Delay", StringValue ("1us"));
  hostLinks.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("32MB"));
    
  PointToPointHelper aggregationLinks;
  aggregationLinks.SetDeviceAttribute ("EnableInt", BooleanValue (true));
  aggregationLinks.SetDeviceAttribute ("DataRate", StringValue ("400Gbps"));
  aggregationLinks.SetChannelAttribute ("Delay", StringValue ("1us"));
  aggregationLinks.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("32MB"));
    
  /******** Create NetDevices ********/
  NS_LOG_UNCOND("Creating NetDevices...");
  NetDeviceContainer hostTorDevices[nHosts];
  for (int i = 0; i < nHosts; i++)
  {
    hostTorDevices[i] = hostLinks.Install (hostNodes.Get(i), 
                                           torNodes.Get(i/(nHosts/nTors)));
    hostTorDevices[i].Get(0)->SetMtu (mtuBytes);
    hostTorDevices[i].Get(1)->SetMtu (mtuBytes);
  }
   
  NetDeviceContainer torAggDevices[nTors*4];
  for (int i = 0; i < nTors; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      torAggDevices[i*4+j] = aggregationLinks.Install (torNodes.Get(i), 
                                                       aggNodes.Get((i/4)*4+j));
      torAggDevices[i*4+j].Get(0)->SetMtu (mtuBytes);
      torAggDevices[i*4+j].Get(1)->SetMtu (mtuBytes);
    }
  }
  
  NetDeviceContainer aggCoreDevices[nAggSw*4];
  for (int i = 0; i < nAggSw; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      aggCoreDevices[i*4+j] = aggregationLinks.Install (aggNodes.Get(i), 
                                                        coreNodes.Get((i%4)*4+j));
      aggCoreDevices[i*4+j].Get(0)->SetMtu (mtuBytes);
      aggCoreDevices[i*4+j].Get(1)->SetMtu (mtuBytes);
    }
  }
    
  /******** Install Internet Stack ********/
  NS_LOG_UNCOND("Installing Internet Stack...");
  /* Enable multi-path routing */
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_PER_FLOW));
    
  InternetStackHelper stack;
  stack.InstallAll ();
    
  /* Enable and configure traffic control layers */
  TrafficControlHelper tchPfifoFast;
  tchPfifoFast.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", 
                                 "MaxSize", StringValue("1p"));
    
  for (int i = 0; i < nHosts; i++)
  {   
    tchPfifoFast.Install (hostTorDevices[i]);
  }
  for (int i = 0; i < nTors*4; i++)
  {
    tchPfifoFast.Install (torAggDevices[i]);
  }
  for (int i = 0; i < nAggSw*4; i++)
  {
    tchPfifoFast.Install (aggCoreDevices[i]);
  }
   
  /* Set IP addresses of the nodes in the network */
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4Address hostAddresses[nHosts];
    
  Ipv4InterfaceContainer hostTorIfs[nHosts];
  for (int i = 0; i < nHosts; i++)
  {
    hostTorIfs[i] = address.Assign (hostTorDevices[i]);
    hostAddresses[i] = hostTorIfs[i].GetAddress (0);
    NS_LOG_LOGIC("Assigned " << hostAddresses[i] << 
                 " to host " << i);
    address.NewNetwork ();
  }
  
  Ipv4InterfaceContainer torAggIfs[nTors*4];
  for (int i = 0; i < nTors*4; i++)
  {
    torAggIfs[i] = address.Assign (torAggDevices[i]);
    address.NewNetwork ();
  }
    
  Ipv4InterfaceContainer aggCoreIfs[nAggSw*4];
  for (int i = 0; i < nAggSw*4; i++)
  {
    aggCoreIfs[i] = address.Assign (aggCoreDevices[i]);
    address.NewNetwork ();
  }
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    
  /* Define an optional/default parameters for nanoPU modules*/
  NS_LOG_UNCOND("Deploying NanoPU Architectures...");
    
  uint16_t payloadSize = hostTorDevices[0].Get (0)->GetMtu () 
                         - ipv4h.GetSerializedSize () 
                         - inth.GetMaxSerializedSize () 
                         - hpcch.GetSerializedSize ();
  Config::SetDefault("ns3::HpccNanoPuArcht::PayloadSize", 
                     UintegerValue(payloadSize));
  Config::SetDefault("ns3::HpccNanoPuArcht::TimeoutInterval", 
                     TimeValue(MilliSeconds(10)));
  Config::SetDefault("ns3::HpccNanoPuArcht::MaxNTimeouts", 
                     UintegerValue(5));
  Config::SetDefault("ns3::HpccNanoPuArcht::MaxNMessages", 
                     UintegerValue(1000));
  Config::SetDefault("ns3::HpccNanoPuArcht::InitialCredit", 
                     UintegerValue(160));
  Config::SetDefault("ns3::HpccNanoPuArcht::BaseRTT", 
                     DoubleValue(MicroSeconds (13).GetSeconds ()));
  Config::SetDefault("ns3::HpccNanoPuArcht::WinAI", 
                     UintegerValue(80));
  Config::SetDefault("ns3::HpccNanoPuArcht::UtilFactor", 
                     DoubleValue(0.95));
  Config::SetDefault("ns3::HpccNanoPuArcht::MaxStage", 
                     UintegerValue(5));
    
//   Config::SetDefault("ns3::HpccNanoPuArcht::EnableMemOptimizations", 
//                      BooleanValue(true));
   
//   LogComponentEnable ("Config", LOG_LEVEL_ALL);
  std::vector<Ptr<HpccNanoPuArcht>> nanoPuArchts;
  for(int i = 0 ; i < nHosts ; i++)
  {
    nanoPuArchts.push_back(CreateObject<HpccNanoPuArcht>());
    nanoPuArchts[i]->AggregateIntoDevice(hostTorDevices[i].Get (0));
    NS_LOG_INFO("**** NanoPU architecture "<< i <<" is created.");
  }
    
  /* Set the message traces for the Homa clients*/
  Ptr<OutputStreamWrapper> msgStream;
  msgStream = asciiTraceHelper.CreateFileStream (outputTracesFileName);
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::HpccNanoPuArcht/MsgBegin", 
                                MakeBoundCallback(&TraceMsgBegin, msgStream));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::HpccNanoPuArcht/MsgFinish", 
                                MakeBoundCallback(&TraceMsgFinish, msgStream));
//   LogComponentEnable ("Config", LOG_LEVEL_ERROR);
  
  /******** Read the Msg Generation Trace From File ********/
  NS_LOG_UNCOND("Reading Input Trace...");
  int nFlows;
  std::ifstream inputTraceFile;
  inputTraceFile.open (inputTraceFileName);
  NS_LOG_DEBUG ("Reading Msg Generation Trace From: " << inputTraceFileName);
    
  std::string line;
  std::istringstream lineBuffer;
  
  getline (inputTraceFile, line);
  lineBuffer.str (line);
  lineBuffer >> nFlows;
  NS_LOG_INFO (nFlows << " messages are found in the trace.");
    
  int srcHost;
  int dstHost;
  int unknown;
  int dstPort;
  uint32_t flowSize;
  double startTime;
  uint32_t nScheduledMsgs = 0;
  while(getline (inputTraceFile, line)) 
  {
    lineBuffer.clear ();
    lineBuffer.str (line);
    lineBuffer >> srcHost;
    lineBuffer >> dstHost;
    lineBuffer >> unknown;
    lineBuffer >> dstPort;
    lineBuffer >> flowSize;
    lineBuffer >> startTime;
      
    Simulator::Schedule (Seconds (startTime), &SendMsg, 
                         nanoPuArchts[srcHost], hostAddresses[dstHost], 
                         dstPort, flowSize, payloadSize);
     nScheduledMsgs++;
  }
  inputTraceFile.close();
  NS_LOG_DEBUG (nScheduledMsgs << " messages are scheduled.");

  /******** Run the Actual Simulation ********/
  NS_LOG_UNCOND("Running the Simulation...");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}