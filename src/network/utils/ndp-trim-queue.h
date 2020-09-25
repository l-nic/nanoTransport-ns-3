/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 */

#ifndef NDPTRIM_H
#define NDPTRIM_H

#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/ndp-header.h"

namespace ns3 {

/**
 * \ingroup queue
 *
 * \brief A packet queue that trims packets on overflow and prioritizes trimmed/control packets
 */
template <typename Item>
class NdpTrimQueue : public Queue<Item>
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief NdpTrimQueue Constructor
   *
   * Creates a NdpTrim queue with a maximum size of 100 packets by default
   */
  NdpTrimQueue ();

  virtual ~NdpTrimQueue ();

  virtual bool Enqueue (Ptr<Item> item);
  virtual Ptr<Item> Dequeue (void);
  virtual Ptr<Item> Remove (void);
  virtual Ptr<const Item> Peek (void) const;

private:
  using Queue<Item>::begin;
  using Queue<Item>::end;
  using Queue<Item>::DoEnqueue;
  using Queue<Item>::DoDequeue;
  using Queue<Item>::DoRemove;
  using Queue<Item>::DoPeek;
  
  QueueSize m_maxDataSize; //!< maximum amount of data allowed to be in the queue
  void SetMaxDataSize (QueueSize size);
  QueueSize GetMaxDataSize (void) const;

  NS_LOG_TEMPLATE_DECLARE;     //!< redefinition of the log component
};


/**
 * Implementation of the templates declared above.
 */

template <typename Item>
TypeId
NdpTrimQueue<Item>::GetTypeId (void)
{
  static TypeId tid = TypeId (("ns3::NdpTrimQueue<" + GetTypeParamName<NdpTrimQueue<Item> > () + ">").c_str ())
    .SetParent<Queue<Item> > ()
    .SetGroupName ("Network")
    .template AddConstructor<NdpTrimQueue<Item> > ()
    .AddAttribute ("MaxSize",
                   "The max queue size",
                   QueueSizeValue (QueueSize ("100p")),
                   MakeQueueSizeAccessor (&QueueBase::SetMaxSize,
                                          &QueueBase::GetMaxSize),
                   MakeQueueSizeChecker ())
    .AddAttribute ("MaxDataSize",
                   "The max data queue size",
                   QueueSizeValue (QueueSize ("50p")),
                   MakeQueueSizeAccessor (&NdpTrimQueue::SetMaxDataSize,
                                          &NdpTrimQueue::GetMaxDataSize),
                   MakeQueueSizeChecker ())
  ;
  return tid;
}

template <typename Item>
NdpTrimQueue<Item>::NdpTrimQueue () :
  Queue<Item> (),
  NS_LOG_TEMPLATE_DEFINE ("NdpTrimQueue")
{
  NS_LOG_FUNCTION (this);
}

template <typename Item>
NdpTrimQueue<Item>::~NdpTrimQueue ()
{
  NS_LOG_FUNCTION (this);
}

template <typename Item>
bool
NdpTrimQueue<Item>::Enqueue (Ptr<Item> item)
{
  NS_LOG_FUNCTION (this << item);
    
  Ptr<Packet> p = item;
//   Ptr<Packet> p = item->GetPacket ();
//   Address addr = item->GetAddress ();
//   uint16_t prot = item->GetProtocol ();
    
  NdpHeader ndph;
  p->RemoveHeader (ndph);
    
  if (ndph.GetFlags () & NdpHeader::Flags_t::DATA)
  {
    NS_LOG_LOGIC ("Current Queue Size: " << this->GetCurrentSize () <<
                  " Item Size: " << item->GetSize () << 
                  " MaxSize: " << this->GetMaxSize () << 
                  " (" << this << ")");
    if (this->GetCurrentSize () + item + item > this->GetMaxDataSize ())
    {
      NS_LOG_DEBUG (" NdpTrimQueue is full for DATA, trimming the packet.");
      p->RemoveAtEnd (ndph.GetPayloadSize ());
//       IncreaseDroppedBytesBeforeEnqueueStats ((uint64_t) ndph.GetPayloadSize ());
        
      ndph.SetPayloadSize ( (uint16_t) (p->GetSize ()) );
      ndph.SetFlags (NdpHeader::Flags_t::CHOP);
        
//       bandToEnqueue = 0;
    }
    else
    {
      NS_LOG_LOGIC (" PfifoNdpQueueDisc DATA queue accepts a packet.");
//       bandToEnqueue = 1;
    }
  }
  else
  {
//     bandToEnqueue = 0;
  }
  
  p->AddHeader (ndph);
    
    
    
    
    
    
  return DoEnqueue (end (), p);
//   return DoEnqueue (end (), CreateObject<QueueDiscItem>(p, addr, prot));
}

template <typename Item>
Ptr<Item>
NdpTrimQueue<Item>::Dequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<Item> item = DoDequeue (begin ());

  NS_LOG_LOGIC ("Popped " << item);

  return item;
}

template <typename Item>
Ptr<Item>
NdpTrimQueue<Item>::Remove (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<Item> item = DoRemove (begin ());

  NS_LOG_LOGIC ("Removed " << item);

  return item;
}

template <typename Item>
Ptr<const Item>
NdpTrimQueue<Item>::Peek (void) const
{
  NS_LOG_FUNCTION (this);

  return DoPeek (begin ());
}
    
template <typename Item>
void 
NdpTrimQueue<Item>::SetMaxDataSize (QueueSize size)
{
   NS_LOG_FUNCTION (this << size);
 
   // do nothing if the size is null
   if (!size.GetValue ())
     {
       return;
     }
 
   m_maxDataSize = size;
 
   NS_ABORT_MSG_IF (size > this->GetMaxSize (),
                    "The maximum data queue size cannot be greater than the max size");
}

template <typename Item>
QueueSize
NdpTrimQueue<Item>::GetMaxDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_maxDataSize;
}

// The following explicit template instantiation declarations prevent all the
// translation units including this header file to implicitly instantiate the
// NdpTrimQueue<Packet> class and the NdpTrimQueue<QueueDiscItem> class. The
// unique instances of these classes are explicitly created through the macros
// NS_OBJECT_TEMPLATE_CLASS_DEFINE (NdpTrimQueue,Packet) and
// NS_OBJECT_TEMPLATE_CLASS_DEFINE (NdpTrimQueue,QueueDiscItem), which are included
// in ndp-trim-queue.cc
extern template class NdpTrimQueue<Packet>;
extern template class NdpTrimQueue<QueueDiscItem>;

} // namespace ns3

#endif /* NDPTRIM_H */
