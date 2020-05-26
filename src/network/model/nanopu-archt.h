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
#ifndef NANOPU_ARCHT_H
#define NANOPU_ARCHT_H

#include "ns3/object.h"

namespace ns3 {

/**
 * \ingroup network
 * \defgroup nanopu-archt NanoPU Architecture
 */

/**
 * \ingroup nanopu-archt
 *
 * \brief Architecture for devices to replace internet and transport layers
 *
 */
class NanoPuArcht : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  NanoPuArcht ();
  virtual ~NanoPuArcht ();
};
    
} // namespace ns3
#endif