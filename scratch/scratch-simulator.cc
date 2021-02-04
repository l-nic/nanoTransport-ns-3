/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ScratchSimulator");

int 
main (int argc, char *argv[])
{
  LogComponentEnable ("ScratchSimulator", LOG_LEVEL_DEBUG);
    
  NS_LOG_UNCOND ("Scratch Simulator");
    
  Packet p = Packet ();  
  NS_LOG_DEBUG("Memory required for a default NS3 Packet: " <<
               sizeof(p) << " Bytes.");
    
  Packet p1500 = Packet (1500); 
  NS_LOG_DEBUG("Memory required for a 1500 Bytes NS3 Packet: " <<
               sizeof(p1500) << " Bytes.");

//   uint8_t *buffer = reinterpret_cast<const uint8_t*> ("hello");
  Packet pBuffer = Packet(reinterpret_cast<const uint8_t*> ("hello"), 1500);
  NS_LOG_DEBUG("Memory required for a NS3 Packet with real payload: " <<
               sizeof(pBuffer) << " Bytes.");
    
  NodeContainer nodes;
  nodes.Create (2);
    
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1us"));
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);
    
  InternetStackHelper stack;
  stack.Install (nodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
    
  HpccNanoPuArcht hpccNanoPuArcht = HpccNanoPuArcht ();
  Ptr<HpccNanoPuArcht> hpccNanoPuPtr = CreateObject<HpccNanoPuArcht> ();
  hpccNanoPuPtr->AggregateIntoDevice(devices.Get(0));
  NS_LOG_DEBUG("Memory required for a HPCC NanoPUArcht: " <<
               sizeof(hpccNanoPuArcht) << " Bytes.");
    
  NanoPuArchtArbiter arbiter = NanoPuArchtArbiter();
  NS_LOG_DEBUG("Memory required for a NanoPuArchtArbiter: " <<
               sizeof(arbiter) << " Bytes.");
      
  NanoPuArchtPacketize packetize = NanoPuArchtPacketize(hpccNanoPuPtr, 
                                                        hpccNanoPuPtr->GetArbiter());
  NS_LOG_DEBUG("Memory required for a NanoPuArchtPacketize: " <<
               sizeof(packetize) << " Bytes.");
      
  NanoPuArchtEgressTimer egressTimer = NanoPuArchtEgressTimer(hpccNanoPuPtr, 
                                                              hpccNanoPuPtr->GetPacketizationBuffer());
  NS_LOG_DEBUG("Memory required for a NanoPuArchtEgressTimer: " <<
               sizeof(egressTimer) << " Bytes.");
      
  NanoPuArchtReassemble reassemble = NanoPuArchtReassemble(hpccNanoPuPtr);
  NS_LOG_DEBUG("Memory required for a NanoPuArchtReassemble: " <<
               sizeof(reassemble) << " Bytes.");
      
  NanoPuArchtIngressTimer ingressTimer = NanoPuArchtIngressTimer(hpccNanoPuPtr, 
                                                                 hpccNanoPuPtr->GetReassemblyBuffer());
  NS_LOG_DEBUG("Memory required for a NanoPuArchtIngressTimer: " <<
               sizeof(ingressTimer) << " Bytes.");
      
  HpccNanoPuArchtPktGen pktGen = HpccNanoPuArchtPktGen(hpccNanoPuPtr);
  NS_LOG_DEBUG("Memory required for a HpccNanoPuArchtPktGen: " <<
               sizeof(pktGen) << " Bytes.");
      
  HpccNanoPuArchtEgressPipe egressPipe = HpccNanoPuArchtEgressPipe(hpccNanoPuPtr);
  NS_LOG_DEBUG("Memory required for a HpccNanoPuArchtEgressPipe: " <<
               sizeof(egressPipe) << " Bytes.");
      
  HpccNanoPuArchtIngressPipe ingressPipe = HpccNanoPuArchtIngressPipe(hpccNanoPuPtr);
  NS_LOG_DEBUG("Memory required for a HpccNanoPuArchtIngressPipe: " <<
               sizeof(ingressPipe) << " Bytes.");
  
  Simulator::Run ();
  Simulator::Destroy ();
}
