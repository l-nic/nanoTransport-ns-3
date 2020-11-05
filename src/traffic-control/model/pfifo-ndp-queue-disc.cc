/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2014 University of Washington
 *               2015 Universita' degli Studi di Napoli Federico II
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
 * Authors:  Stefano Avallone <stavallo@unina.it>
 *           Tom Henderson <tomhend@u.washington.edu>
 */

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/socket.h"
#include "pfifo-ndp-queue-disc.h"
#include "ns3/network-module.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PfifoNdpQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (PfifoNdpQueueDisc);

TypeId PfifoNdpQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PfifoNdpQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<PfifoNdpQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc.",
                   QueueSizeValue (QueueSize ("1000p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
  ;
  return tid;
}

PfifoNdpQueueDisc::PfifoNdpQueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::PACKETS)
{
  NS_LOG_FUNCTION (this);
}

PfifoNdpQueueDisc::~PfifoNdpQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

bool
PfifoNdpQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
    
  NS_LOG_DEBUG("*** The corresponding net device queue: " << GetNetDeviceQueueInterface ()->GetTxQueue (0));
    
  bool retval = false;
  uint32_t bandToEnqueue;

  Ptr<Packet> p = item->GetPacket ();
  NdpHeader ndph;
  p->RemoveHeader (ndph);
    
  if (ndph.GetFlags () & NdpHeader::Flags_t::DATA)
  {
    NS_LOG_DEBUG("Num Packets in Data queue: " << GetCurrentSize () <<
                 " Max Size: " << GetMaxSize () <<
                 " (" << GetInternalQueue (1) << ", " << GetNetDeviceQueueInterface() << ")");
    if (GetInternalQueue (1)->GetNPackets () +1 >= GetMaxSize ().GetValue ())
    {
      NS_LOG_LOGIC (Simulator::Now ().GetNanoSeconds () << 
                    " PfifoNdpQueueDisc DATA queue is full, trimming the packet.");
      p->RemoveAtEnd (ndph.GetPayloadSize ());
      IncreaseDroppedBytesBeforeEnqueueStats ((uint64_t) ndph.GetPayloadSize ());
        
      ndph.SetPayloadSize ( (uint16_t) (p->GetSize ()) );
      ndph.SetFlags (NdpHeader::Flags_t::DATA | NdpHeader::Flags_t::CHOP);
        
      bandToEnqueue = 0;
    }
    else
    {
      NS_LOG_LOGIC (Simulator::Now ().GetNanoSeconds () << 
                    " PfifoNdpQueueDisc DATA queue accepts a packet.");
      bandToEnqueue = 1;
    }
  }
  else
  {
    bandToEnqueue = 0;
  }
  
  p->AddHeader (ndph);
      
  if (GetInternalQueue (bandToEnqueue)->GetNPackets () >= GetMaxSize ().GetValue ())
    {
      NS_LOG_LOGIC ("Queue disc limit exceeded -- dropping packet");
      DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
      return false;
    }

  retval = GetInternalQueue (bandToEnqueue)->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::DropBeforeEnqueue is called by the
  // internal queue because QueueDisc::AddInternalQueue sets the trace callback

  if (!retval)
    {
      NS_LOG_WARN ("Packet enqueue failed. Check the size of the internal queues");
    }

  NS_LOG_LOGIC ("Number of packets in band " << bandToEnqueue << ": " 
                << GetInternalQueue (bandToEnqueue)->GetNPackets ());

  return retval;
}

Ptr<QueueDiscItem>
PfifoNdpQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Dequeue ()) != 0)
        {
          NS_LOG_LOGIC ("Popped. Remaining packets in band " << 
                        i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }
  
  NS_LOG_LOGIC ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
PfifoNdpQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Peek ()) != 0)
        {
          NS_LOG_LOGIC ("Peeked from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
PfifoNdpQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("PfifoNdpQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () != 0)
    {
      NS_LOG_ERROR ("PfifoNdpQueueDisc needs no packet filter");
      return false;
    }

  if (GetNInternalQueues () == 0)
    {
      // create 2 DropTail queues with GetLimit() packets each
      ObjectFactory factory;
      factory.SetTypeId ("ns3::DropTailQueue<QueueDiscItem>");
      factory.Set ("MaxSize", QueueSizeValue (GetMaxSize ()));
      AddInternalQueue (factory.Create<InternalQueue> ());
      AddInternalQueue (factory.Create<InternalQueue> ());
    }

  if (GetNInternalQueues () != 2)
    {
      NS_LOG_ERROR ("PfifoNdpQueueDisc needs 2 internal queues");
      return false;
    }

  if (GetInternalQueue (0)-> GetMaxSize ().GetUnit () != QueueSizeUnit::PACKETS ||
      GetInternalQueue (1)-> GetMaxSize ().GetUnit () != QueueSizeUnit::PACKETS )
    {
      NS_LOG_ERROR ("PfifoNdpQueueDisc needs 2 internal queues operating in packet mode");
      return false;
    }

  for (uint8_t i = 0; i < 1; i++)
    {
      if (GetInternalQueue (i)->GetMaxSize () < GetMaxSize ())
        {
          NS_LOG_ERROR ("The capacity of some internal queue(s) is less than the queue disc capacity");
          return false;
        }
    }

  return true;
}

void
PfifoNdpQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
}

} // namespace ns3
