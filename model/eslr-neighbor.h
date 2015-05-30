/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Hiroaki Nishi Laboratory, Keio University, Japan
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Janaka Wijekoon <janaka@west.sd.ekio.ac.jp>, Hiroaki Nishi <west@sd.keio.ac.jp>
 */

#ifndef ESLR_NEIGHBOR_H
#define ESLR_NEIGHBOR_H

#include <cassert>
#include <list>
#include <sys/types.h>

#include "ns3/eslr-definition.h"
#include "ns3/eslr-headers.h"
#include "ns3/eslr-route.h"

#include "ns3/ipv4.h"
#include "ns3/ipv4-route.h"
#include "ns3/timer.h"
#include "ns3/net-device.h"
#include "ns3/output-stream-wrapper.h"

namespace ns3 {
namespace eslr {

class NeighborTableEntry
{
public:
  //NeighborTableEntry (void);
  /**
  * \brief Constructor
  * \param neighborID ID of the neighbor hashvalue of (AS + IF0's address, IF0's netmask)
  * \param neighborAddress IP address of the sending interface
  * \param neighborMask network mask of the sending interface
  * \param interface received interface index
  * \param socket bound socket
  * \param neighborAuthType authentication type that neighbor suggested to use (plaintext = 0, MD5 = 1, SHA = 2)
  * \param neighborAuthData the authentication phrase
  */
  NeighborTableEntry (uint32_t neighborID = 0, 
                      Ipv4Address neighborAddress = Ipv4Address (),
                      Ipv4Mask neighborMask = Ipv4Mask (), 
                      uint32_t interface = 0, 
                      Ptr<Socket> socket = 0,
                      eslr::AuthType authType = eslr::PLAIN_TEXT,
                      uint16_t authData = 0,
											eslr::Validity validity = eslr::INVALID);

  ~NeighborTableEntry ();
  
  /**
  * \brief Get and Set the neighbor ID
  * \param neighborID the neighbor ID
  * \returns the neighbor ID
  */
  void SetNeighborID (uint32_t neighborID)
  {
    m_neighborID = neighborID;
  }
  uint32_t GetNeighborID () const
  {
    return m_neighborID;
  }

  /**
  * \brief Get and Set the neighbor's IP address
  * \param neighborAddress the neighbor's IP address
  * \returns the neighbor's IP address
  */
  void SetNeighborAddress (Ipv4Address neighborAddress)
  {
    m_neighborAddress = neighborAddress;
  }
  Ipv4Address GetNeighborAddress () const
  {
    return m_neighborAddress;
  }

  /**
  * \brief Get and Set the neighbor's network mask
  * \param neighborAddress the neighbor's network mask
  * \returns the neighbor's network mask
  */
  void SetNeighborMask (Ipv4Mask neighborMask)
  {
    m_neighborMask = neighborMask;
  }
  Ipv4Mask GetNeighborMask () const
  {
    return m_neighborMask;
  }

  /**
  * \brief Get and Set the interface 
  * \param neighborID the interface 
  * \returns the interface 
  */
  void SetInterface (uint32_t interface)
  {
    m_interface = interface;
  }
  uint32_t GetInterface () const
  {
    return m_interface;
  }

  /**
  * \brief Get and Set the bounded socket
  * \param neighborAddress the bounded socket
  * \returns the bounded socket
  */
  void SetSocket (Ptr<Socket> socket)
  {
    m_socket = socket;
  }
  Ptr<Socket> GetSocket () const
  {
    return m_socket;
  }

  /**
  * \brief Get and Set the authentication type 
  * \param neighborID the authentication type  
  * \returns the authentication type  
  */
  void SetAuthType (eslr::AuthType authType)
  {
    m_authType = authType;
  }
  AuthType GetAuthType () const
  {
    return eslr::AuthType(m_authType);
  }

  /**
  * \brief Get and Set the authentication data 
  * \param neighborID the authentication data  
  * \returns the authentication data  
  */
  void SetAuthData (uint16_t authData)
  {
    m_authData = authData;
  }
  uint16_t GetAuthData () const
  {
    return m_authData;
  }

  /**
  * \brief Get and Set the validity of the neighbor record 
  * \param validity the validity of the neighbor record  
  * \returns the validity of the neighbor record
  */
  void SetValidity (eslr::Validity validity)
  {
    m_validity = validity;
  }
  eslr::Validity GetValidity () const
  {
    return eslr::Validity (m_validity);
  }

private:
  uint32_t m_neighborID;  //!< ID of the neighbor
  Ipv4Address m_neighborAddress; //!< address of the Neighbor (this is the sender's interface IP address')
  Ipv4Mask m_neighborMask;  //!< mask of the sender's IP address'
  uint32_t m_interface;  //!< the neighbor is connected via this interface
  Ptr<Socket> m_socket; //!< the neighbor is sending data to this socket (Broadcase listning socket)
  eslr::AuthType m_authType; //!< authentication type 
  uint16_t m_authData; //!< authentication phrase
	eslr::Validity m_validity; //!< validity of the neighbor record
  
}; // end of class NeighborTableEntry

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param neighbor the neighbor table entry
 * \returns the reference to the output stream
 */
std::ostream& operator<< (std::ostream& os, NeighborTableEntry const& neighbor);


class NeighborTable
{
public:
  /// Container for a neighbor table entry 
  typedef std::pair <NeighborTableEntry, EventId> NeighborTableRecord;
  
  /// Container for an instance of the neighbor table
  typedef std::list<std::pair <NeighborTableEntry*, EventId> > NeighborTableInstance;

  /// Iterator for the Neighbor table entry container
  typedef std::list<std::pair <NeighborTableEntry*, EventId> >::iterator NeighborI;

  /// Constant Iterator for the Neighbor table entry container
  typedef std::list<std::pair <NeighborTableEntry*, EventId> >::const_iterator NeighborCI;

  NeighborTable ();
  ~NeighborTable ();

  /**
  * \brief Add a new neighbor record to the neighbor table 
  * \param neighborEntry neighbor details
  * \param invalidateTime time that the particular route invalidate
  * \param deleteTime garbage collection delay
  * \param routingTable a reference to the routing table.
  * \param timeoutDelay the timeout delay of the routes
  * \param garbageCollectionDelay the garbage collection delay for the route records
  * \param settlingDelay settling timeout for the route records
  * this is added, because at the time that a neighbor record will be invalidated, all route records that 
  * refer the neighbor as the gatewas have to be invalidated as well. However, Add, Update methods are
  * coupled with the neighbor invalidate method. Therefore, at the begening (i.e., add method) the 
  * reference point to the routing table is passed to the invalidate method.  
  */
  void AddNeighbor (NeighborTableEntry *neighborEntry, Time invalidateTime, Time deleteTime, RoutingTable& routingTable, Time timeoutDelay, Time garbageCollectionDelay, Time settlingDelay);

  /**
  * \brief Delete a neighbor record from the neighbor table after NbrRemove secondes exceeds
  * \param neighborEntry neighbor details   
  * \param routingTable a routing table instance
  * \returns true if success  
  */
  bool DeleteNeighbor (NeighborTableEntry *neighborEntry, RoutingTable& routingTable);

  /**
  * \brief Invalidate a neighbor record after NbrTimeout seconds exceeedes
  * \param neighborEntry neighbor details
  * \param deleteTime garbage collection delay
  * \param invalidateTime route invalidation delay
  * \param routingTable a reference to the routing table.
  * this is added, because at the time that a neighbor record will be invalidated, all route records that 
  * refer the neighbor as the gatewas have to be invalidated as well. However, Add, Update methods are
  * coupled with the neighbor invalidate method. Therefore, at the begening (i.e., add method) the 
  * reference point to the routing table is passed to the invalidate method. 
  * \returns true if success  
  */
  bool InvalidateNeighbor (NeighborTableEntry *neighborEntry, Time invalidateTime, Time deleteTime, RoutingTable& routingTable);

  /**
  * \brief Find and return a neighbor record if exist. Do not consider the VALID flag
  * \param neighborID ffind for the ID   
  * \returns true and the currosponding neighbor record if success  
  */
  bool FindNeighbor (uint32_t neighborID, NeighborI &retNeighborEntry);

	/**
  * \brief Find and return a neighbor record of a valid neighbor for given ID (consider the VALID flag)
  * \param neighborID find for the ID   
  * \returns true and the currosponding neighbor record if success  
  */
  bool FindValidNeighbor (uint32_t neighborID, NeighborI &retNeighborEntry);

	/**
  * \brief Find and return a neighbor record of a valid neighbor for given address (consider the VALID flag)
  * \param address find for the address   
  * \returns true and the currosponding neighbor record if success  
  */
	bool FindValidNeighborForAddress (Ipv4Address address, NeighborI &retNeighborEntry);

  /**
  * \brief Update a given neighbor Record
  * \param neighborEntry neighbor details   
  * \param invalidateTime time that the particular route invalidate
  * \param deleteTime garbage collection delay
  * \param routingTable a reference to the routing table.
  * this is added, because at the time that a neighbor record will be invalidated, all route records that 
  * refer the neighbor as the gatewas have to be invalidated as well. However, Add, Update methods are
  * coupled with the neighbor invalidate method. Therefore, at the begening (i.e., add method) the 
  * reference point to the routing table is passed to the invalidate method. 
  * \returns true if success  
  */
  bool UpdateNeighbor (NeighborTableEntry *neighborEntry, Time invalidateTime, Time deleteTime, RoutingTable& routingTable);

  /**
  * \brief Return an instance of the neighbor table. This is specifically implemented for debug purposes
  * \param neighborTableInstance an instance of the std::list <NeighborTableEntry*, EventID>   
  * \returns true if success  
  */
  void ReturnNeighborTable (NeighborTableInstance &instance);

  /**
  * \brief Print the neighbor Table
  * \param the output stream 
  */
  void PrintNeighborTable (Ptr<OutputStreamWrapper> stream) const;

  /**
  * \brief return if the neighboer table is empty or not
  */
  bool IsEmpty(void);

  /**
  * \brief clear the neighbor table
  */
	void DoDispose ()
	{
		m_neighborTable.clear ();
	}

	private:
  NeighborTableInstance m_neighborTable; //!< instance of the neighbor table
  Time m_routeTimeoutDelay; //!< Delay that determines the neighbor is UNRESPONSIVE
  Time m_routeGarbageCollectionDelay; //!< Delay before remove UNRESPONSIVE route/neighbor record
  Time m_routeSettlingDelay; //!< Delay that determins a particular route is stable
}; // end of class NeighborTable

} // end of eslr namespace
} // end of ns3 namespace
#endif /* NEIGHBOR */
