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

#include <functional>
#include <queue>
#include <vector>
#include <iostream>

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
    
//   Packet p = Packet ();  
//   NS_LOG_DEBUG("Memory required for a default NS3 Packet: " <<
//                sizeof(p) << " Bytes.");
    
//   Packet p1500 = Packet (1500); 
//   NS_LOG_DEBUG("Memory required for a 1500 Bytes NS3 Packet: " <<
//                sizeof(p1500) << " Bytes.");

// //   uint8_t *buffer = reinterpret_cast<const uint8_t*> ("hello");
//   Packet pBuffer = Packet(reinterpret_cast<const uint8_t*> ("hello"), 1500);
//   NS_LOG_DEBUG("Memory required for a NS3 Packet with real payload: " <<
//                sizeof(pBuffer) << " Bytes.");
    
//   NodeContainer nodes;
//   nodes.Create (2);
    
//   PointToPointHelper pointToPoint;
//   pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
//   pointToPoint.SetChannelAttribute ("Delay", StringValue ("1us"));
//   NetDeviceContainer devices;
//   devices = pointToPoint.Install (nodes);
    
//   InternetStackHelper stack;
//   stack.Install (nodes);
//   Ipv4AddressHelper address;
//   address.SetBase ("10.1.1.0", "255.255.255.0");
//   Ipv4InterfaceContainer interfaces = address.Assign (devices);
    
//   HpccNanoPuArcht hpccNanoPuArcht = HpccNanoPuArcht ();
//   Ptr<HpccNanoPuArcht> hpccNanoPuPtr = CreateObject<HpccNanoPuArcht> ();
//   hpccNanoPuPtr->AggregateIntoDevice(devices.Get(0));
//   NS_LOG_DEBUG("Memory required for a HPCC NanoPUArcht: " <<
//                sizeof(hpccNanoPuArcht) << " Bytes.");
    
//   NanoPuArchtArbiter arbiter = NanoPuArchtArbiter();
//   NS_LOG_DEBUG("Memory required for a NanoPuArchtArbiter: " <<
//                sizeof(arbiter) << " Bytes.");
      
//   NanoPuArchtPacketize packetize = NanoPuArchtPacketize(hpccNanoPuPtr, 
//                                                         hpccNanoPuPtr->GetArbiter());
//   NS_LOG_DEBUG("Memory required for a NanoPuArchtPacketize: " <<
//                sizeof(packetize) << " Bytes.");
      
//   NanoPuArchtEgressTimer egressTimer = NanoPuArchtEgressTimer(hpccNanoPuPtr, 
//                                                               hpccNanoPuPtr->GetPacketizationBuffer());
//   NS_LOG_DEBUG("Memory required for a NanoPuArchtEgressTimer: " <<
//                sizeof(egressTimer) << " Bytes.");
      
//   NanoPuArchtReassemble reassemble = NanoPuArchtReassemble(hpccNanoPuPtr);
//   NS_LOG_DEBUG("Memory required for a NanoPuArchtReassemble: " <<
//                sizeof(reassemble) << " Bytes.");
      
//   NanoPuArchtIngressTimer ingressTimer = NanoPuArchtIngressTimer(hpccNanoPuPtr, 
//                                                                  hpccNanoPuPtr->GetReassemblyBuffer());
//   NS_LOG_DEBUG("Memory required for a NanoPuArchtIngressTimer: " <<
//                sizeof(ingressTimer) << " Bytes.");
      
//   HpccNanoPuArchtPktGen pktGen = HpccNanoPuArchtPktGen(hpccNanoPuPtr);
//   NS_LOG_DEBUG("Memory required for a HpccNanoPuArchtPktGen: " <<
//                sizeof(pktGen) << " Bytes.");
      
//   HpccNanoPuArchtEgressPipe egressPipe = HpccNanoPuArchtEgressPipe(hpccNanoPuPtr);
//   NS_LOG_DEBUG("Memory required for a HpccNanoPuArchtEgressPipe: " <<
//                sizeof(egressPipe) << " Bytes.");
      
//   HpccNanoPuArchtIngressPipe ingressPipe = HpccNanoPuArchtIngressPipe(hpccNanoPuPtr);
//   NS_LOG_DEBUG("Memory required for a HpccNanoPuArchtIngressPipe: " <<
//                sizeof(ingressPipe) << " Bytes.");
    
  /**********************************************************/
    
  struct HighRankFirst
  {
    bool operator()(const Ptr<Packet> lhs, const Ptr<Packet> rhs) const
    {
      SocketPriorityTag lhsTag;
      lhs->PeekPacketTag(lhsTag);
      uint8_t lhsRank = lhsTag.GetPriority ();
      
      SocketPriorityTag rhsTag;
      rhs->PeekPacketTag(rhsTag);
      uint8_t rhsRank = rhsTag.GetPriority ();
      
      return lhsRank > rhsRank;
    }
  };
    
  Ptr<Packet> p0 = Create<Packet> ();
  SocketPriorityTag tag0;
  uint8_t rank0 = 0;
  tag0.SetPriority(rank0);
  p0->AddPacketTag (tag0);
  NS_LOG_DEBUG("Packet with rank " << (int)rank0 << ": " << p0);
    
  Ptr<Packet> p1 = Create<Packet> ();
  SocketPriorityTag tag1;
  uint8_t rank1 = 1;
  tag1.SetPriority(rank1);
  p1->AddPacketTag (tag1);
  NS_LOG_DEBUG("Packet with rank " << (int)rank1 << ": " << p1);
    
  Ptr<Packet> p2 = Create<Packet> ();
  SocketPriorityTag tag2;
  uint8_t rank2 = 2;
  tag2.SetPriority(rank2);
  p2->AddPacketTag (tag2);
  NS_LOG_DEBUG("Packet with rank " << (int)rank2 << ": " << p2);
    
  Ptr<Packet> p3 = Create<Packet> ();
  SocketPriorityTag tag3;
  uint8_t rank3 = 3;
  tag3.SetPriority(rank3);
  p3->AddPacketTag (tag3);
  NS_LOG_DEBUG("Packet with rank " << (int)rank3 << ": " << p3);
    
  Ptr<Packet> p4 = Create<Packet> ();
  SocketPriorityTag tag4;
  p4->PeekPacketTag(tag4);
  NS_LOG_DEBUG("Packet without rank: " << p4 <<
               " Default priority: " << (int)tag4.GetPriority());
    
  std::priority_queue<Ptr<Packet>, std::vector<Ptr<Packet>>, HighRankFirst> pq;
  pq.push(p3);
  pq.push(p1);
  pq.push(p0);
  pq.push(p4);
  pq.push(p2);
    
  while(!pq.empty()) 
  {
    NS_LOG_DEBUG("Top Packet in the priority queue: " << pq.top());
    pq.pop();
  }
    
  /**********************************************************/
  
//   Simulator::Run ();
//   Simulator::Destroy ();
    
}
