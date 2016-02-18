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
  * \param neighborID ID of the neighbor: 
	* 			\\TODO 
	* 			  hash value of (AS + IF0's address, IF0's net-mask)
	* 			Now it is the node ID (ns3::node::id)
  * \param neighborAddress IP address of the sending interface
  * \param neighborMask network mask of the sending interface
  * \param interface received interface index
  * \param socket bound socket
  * \param neighborAuthType authentication type that neighbor suggested to use 
	* 				\\TODO 
	* 				  (plain-text = 0, MD5 = 1, SHA = 2)
	* 				Now supports only plain text
  * \param neighborAuthData the authentication phrase
  */
  NeighborTableEntry (uint16_t neighborID = 0, 
                      Ipv4Address neighborAddress = Ipv4Address (),
                      Ipv4Mask neighborMask = Ipv4Mask (), 
                      uint32_t interface = 0, 
                      Ptr<Socket> socket = 0,
                      eslr::AuthType authType = eslr::PLAIN_TEXT,
                      uint16_t authData = 0,
											uint8_t identifier = 0,
											eslr::Validity validity = eslr::INVALID);

  ~NeighborTableEntry ();
  
  /**
  * \brief Get and Set the neighbor ID
  * \param neighborID the neighbor ID
  * \returns the neighbor ID
  */
  void SetNeighborID (uint16_t neighborID)
  {
    m_neighborID = neighborID;
  }
  uint16_t GetNeighborID () const
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
  * \param interface the interface 
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
  * \param socket the bounded socket
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
  * \param authType the authentication type  
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
  * \param authData the authentication data  
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

  /**
  * \brief Get and Set the identification number
  * \param identifier the identifier number
  * \returns the identifier number
  */
  void SetAuthData (uint8_t identifier)
  {
    m_identifier = identifier;
  }
  uint8_t GetIdentifier () const
  {
    return m_identifier;
  }

private:
  uint16_t m_neighborID;  //!< ID of the neighbor
  Ipv4Address m_neighborAddress; //!< address of the Neighbor (this is the sender's interface IP address')
  Ipv4Mask m_neighborMask;  //!< mask of the sender's IP address'
  uint32_t m_interface;  //!< the neighbor is connected via this interface
  Ptr<Socket> m_socket; //!< the neighbor is sending data to this socket (Broadcast listing socket)
  eslr::AuthType m_authType; //!< authentication type 
  uint16_t m_authData; //!< authentication phrase
	uint8_t m_identifier; //!< the random identifier between the neighbor
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
	 * \brief Add a void neighbor.
	 * At the beginning of the neighbor discovery process, The neighbors are added to the neighbor
	 * table using a VOID state. They will remain under that state until the node receives a 
	 * RRQ message from the neighbor. When the node receives a RRQ message from the newly discovered neighbor
	 * the node will update the state to VALID*/
	void AddVoidNeighbor (NeighborTableEntry *neighborEntry, 
			Time removeTime);

	/**
	 * \brief Each VOID neighbor is coupled with and event to delete the neighbor record after a waiting time. 
	 * The waiting time is depending on the configuration
	 */
	bool DeleteVoidNeighbor (NeighborTableEntry *neighborEntry);

	/**
	 * \brief find a neighbor record with a void state
	 * \parem id id of the neighbor that mush find */
	bool FindVoidNeighbor (uint16_t id);

	/***
	 * \brief Find and void neighbor for a given IP address
	 * \param address the IP address of the selecting neighbor
	 * \param retneighborentry returning neighbor record */
	bool FindVoidNeighborForAddress (Ipv4Address address, 
			NeighborI &retNeighborEntry);

  /**
  * \brief Add a new neighbor record to the neighbor table 
  * \param neighborEntry neighbor details
  * \param invalidateTime time that the particular route invalidate
  * \param deleteTime garbage collection delay
  */
  void AddNeighbor (NeighborTableEntry *neighborEntry, 
			Time invalidateTime, 
			Time deleteTime);

  /**
  * \brief Delete a neighbor record from the neighbor table after NbrRemove seconds exceeds
  * \param neighborEntry neighbor details   
  * \returns true if success  
  */
  bool DeleteNeighbor (NeighborTableEntry *neighborEntry);

  /**
  * \brief Invalidate a neighbor record after NbrTimeout seconds exceeds
  * \param neighborEntry neighbor details
  * \param deleteTime garbage collection delay
  */
  bool InvalidateNeighbor (NeighborTableEntry *neighborEntry, 
			Time deleteTime);

  /**
  * \brief Find and return a neighbor record for ID. Do not consider the VALID flag
  * \param neighborID find for the ID   
  * \returns true and the corresponding neighbor record if success  
  */
  bool FindNeighbor (uint32_t neighborID, 
			NeighborI &retNeighborEntry);

	/**
  * \brief Find and return a neighbor record for a given ID. Consider the VALID flag
  * \param neighborID find for the ID   
  * \returns true and the corresponding neighbor record if success  
  */
  bool FindValidNeighbor (uint32_t neighborID, 
			NeighborI &retNeighborEntry);

	/**
  * \brief Find and return a neighbor record for given address. Consider the VALID flag
  * \param address find for the address   
  * \returns true and the corresponding neighbor record if success  
  */
	bool FindValidNeighborForAddress (Ipv4Address address, 
			NeighborI &retNeighborEntry);

  /**
  * \brief Find and return a neighbor record for a given address. Do not consider the VALID flag
  * \param address find for the address
  * \returns true and the corresponding neighbor record if success
  */
	bool FindNeighborForAddress (Ipv4Address address, 
			NeighborI &retNeighborEntry);

  /**
  * \brief Update a given neighbor Record
  * \param neighborEntry neighbor details   
  * \param invalidateTime time that the particular route invalidate
  * \param deleteTime garbage collection delay
  */
  bool UpdateNeighbor (NeighborTableEntry *neighborEntry, 
			Time invalidateTime, 
			Time deleteTime);

  /**
  * \brief Return an instance of the neighbor table. This is specifically implemented for debug purposes
	* 				The one who call this method, should clear the temporary neighbor table at the place 
	* 				where this method is called. That is a best practice for good memory management.
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
  * \brief return if the neighbor table is empty or not
  */
  bool IsEmpty(void);

  /**
  * \brief clear the neighbor table
  */
	void DoDispose ()
	{
		m_neighborTable.clear ();
	}

	void DoInitialize (RoutingTable& routingTablei, 
			Time routeTimeout, 
			Time routeDelete, 
			Time routeSettling)
	{
		m_routeTimeoutDelay = routeTimeout;
		m_routeGarbageCollectionDelay = routeDelete;
		m_routeSettlingDelay = routeSettling;
		routeInstance = routingTablei;
	}

	private:
  NeighborTableInstance m_neighborTable; //!< instance of the neighbor table
  Time m_routeTimeoutDelay; //!< Delay that determines the neighbor is UNRESPONSIVE
  Time m_routeGarbageCollectionDelay; //!< Delay before remove UNRESPONSIVE route/neighbor record
  Time m_routeSettlingDelay; //!< Delay that determines a particular route is stable
	RoutingTable routeInstance; //!< the instance of both routing tables
}; // end of class NeighborTable

} // end of eslr namespace
} // end of ns3 namespace
#endif /* NEIGHBOR */
