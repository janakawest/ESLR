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

#include <iomanip>

#include "eslr-neighbor.h"

#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/node-list.h"
#include "ns3/node.h"
#include "ns3/timer.h"


NS_LOG_COMPONENT_DEFINE ("ESLRNeighborTable");

//#define NbrTimeout 90
//#define NbrRemove 60

namespace ns3 {
namespace eslr {
/* 
 * Neighbor Table Entry
*/

  NeighborTableEntry::NeighborTableEntry (uint32_t neighborID, 
                      Ipv4Address neighborAddress,
                      Ipv4Mask neighborMask, 
                      uint32_t interface, 
                      Ptr<Socket> socket,
                      eslr::AuthType authType,
                      uint16_t authData,
											eslr::Validity validity ): m_neighborID (neighborID),
                                          m_neighborAddress (neighborAddress),
                                          m_neighborMask (neighborMask),
                                          m_interface (interface),
                                          m_socket (socket),
                                          m_authType (authType),
                                          m_authData (authData),
																					m_validity (validity)
  {
    // Constructor
  }

  NeighborTableEntry::~NeighborTableEntry()
  {
    // Destructor
  }

/* 
 * Neighbor Table
*/

  NeighborTable::NeighborTable () 
  {
    m_routeTimeoutDelay = Seconds (0); //!< Delay that determines the neighbor is UNRESPONSIVE
    m_routeGarbageCollectionDelay = Seconds (0); //!< Delay before remove UNRESPONSIVE route/neighbor record
    m_routeSettlingDelay = Seconds (0); //!< Delay that determins a particular route is stable
  }
  NeighborTable::~NeighborTable (){/*Destructor*/}

  bool 
  NeighborTable::IsEmpty ()
  {
	  return (m_neighborTable.empty ())?true:false;
  }

  void 
  NeighborTable::AddNeighbor (NeighborTableEntry *neighborEntry, Time invalidateTime, Time deleteTime, RoutingTable& routingTable, Time timeoutDelay, Time garbageCollectionDelay, Time settlingDelay)
  {    
    NS_LOG_FUNCTION ("Added a new Neighbor " << neighborEntry->GetNeighborID());

		//std::cout << "Add Neighbor neighbor " << neighborEntry->GetNeighborID () << ":" << neighborEntry->GetNeighborAddress () << std::endl;
    EventId invalidateEvent = Simulator::Schedule (invalidateTime, &NeighborTable::InvalidateNeighbor, this, neighborEntry, invalidateTime, deleteTime, routingTable);
    m_neighborTable.push_front(std::make_pair (neighborEntry,invalidateEvent));

    m_routeTimeoutDelay = timeoutDelay;
    m_routeGarbageCollectionDelay = garbageCollectionDelay;
    m_routeSettlingDelay = settlingDelay;
  }

  bool 
  NeighborTable::UpdateNeighbor (NeighborTableEntry *neighborEntry, Time invalidateTime, Time deleteTime, RoutingTable& routingTable)
  {
    NS_LOG_FUNCTION ("Update the neighbor " << neighborEntry->GetNeighborID());

    bool retVal = false;
    NeighborI it;

    for (it = m_neighborTable.begin ();  it!= m_neighborTable.end (); it++)
    {
      if ((it->first->GetNeighborID () == neighborEntry->GetNeighborID ()) && (it->first->GetNeighborAddress () == neighborEntry->GetNeighborAddress ()))
      {
				//std::cout << "Update Neighbor neighbor " << it->first->GetNeighborID () << ":" << it->first->GetNeighborAddress () << std::endl;
        delete it->first;
        it->first = neighborEntry;
        it->second.Cancel ();
        it->second = Simulator::Schedule (invalidateTime, &NeighborTable::InvalidateNeighbor, this, it->first, invalidateTime, deleteTime, routingTable);
        retVal = true;
        break;
      }
    }
    if (!retVal)
    {
      NS_LOG_LOGIC ("Neighbor not available: " << int (neighborEntry->GetNeighborID ()));
      return retVal;        
    }

    return retVal;
  }

  bool 
  NeighborTable::InvalidateNeighbor (NeighborTableEntry *neighborEntry, Time invalidateTime, Time deleteTime, RoutingTable& routingTable)
  {
    NS_LOG_FUNCTION ("Invalidate the neighbor " << neighborEntry->GetNeighborID());

    bool retVal = false;
    NeighborI it;

    for (it = m_neighborTable.begin ();  it!= m_neighborTable.end (); it++)
    {
      if ((it->first->GetNeighborID () == neighborEntry->GetNeighborID ()) && (it->first->GetNeighborAddress () == neighborEntry->GetNeighborAddress ()))
      {
        NS_LOG_FUNCTION ("Invalidate route records that referes " << neighborEntry->GetNeighborID ());

				it->first->SetValidity (eslr::INVALID);
        if (it->second.IsRunning ())
          it->second.Cancel ();
        it->second = Simulator::Schedule (deleteTime, &NeighborTable::DeleteNeighbor, this, it->first, routingTable);
        retVal = true;
        break;
      }
    }
    if (!retVal)
    {
      NS_LOG_LOGIC ("Neighbor not available: " << int (neighborEntry->GetNeighborID ()));
      return retVal;        
    }
    return retVal;
  }

  bool 
  NeighborTable::DeleteNeighbor (NeighborTableEntry *neighborEntry, RoutingTable& routingTable)
  {
    NS_LOG_FUNCTION ("Delete the neighbor " << neighborEntry->GetNeighborID());

    bool retVal = false;
    NeighborI it;

    for (it = m_neighborTable.begin ();  it!= m_neighborTable.end (); it++)
    {
      if ((it->first->GetNeighborID () == neighborEntry->GetNeighborID ()) && (it->first->GetNeighborAddress () == neighborEntry->GetNeighborAddress ()))
      {
        // Along with the deleting neighbor,
        // invalidate all route records refering the neighbor as the gateway
        routingTable.InvalidateRoutesForGateway (neighborEntry->GetNeighborAddress (),  
                                                 m_routeTimeoutDelay, m_routeGarbageCollectionDelay,
                                                 m_routeSettlingDelay, eslr::BACKUP);
        routingTable.InvalidateRoutesForGateway (neighborEntry->GetNeighborAddress (),  
                                                 m_routeTimeoutDelay, m_routeGarbageCollectionDelay,
                                                 m_routeSettlingDelay,eslr::MAIN);
                                                       
        delete neighborEntry;
        m_neighborTable.erase (it);
        retVal = true;
        break;
      }
    }
    if (!retVal)
    {
      NS_LOG_LOGIC ("Neighbor not available: " << int (neighborEntry->GetNeighborID ()));
      return retVal;        
    }
    return retVal;
  }

  bool 
  NeighborTable::FindNeighbor (uint32_t neighborID, NeighborI &retNeighborEntry)
  {
    bool retVal = false;
    NeighborI it;
    for (it = m_neighborTable.begin ();  it!= m_neighborTable.end (); it++)
    {
      if ((it->first->GetNeighborID () == neighborID))
      {
        retNeighborEntry = it;
        retVal = true;
        break;
      }
    }
    if (!retVal)
    {
      NS_LOG_LOGIC ("Neighbor not available: " << int (neighborID));
      return retVal;        
    }
    return retVal;
  }

	bool 
  NeighborTable::FindValidNeighbor (uint32_t neighborID, NeighborI &retNeighborEntry)
  {
    bool retVal = false;
    NeighborI it;

    for (it = m_neighborTable.begin ();  it!= m_neighborTable.end (); it++)
    {
      if ((it->first->GetNeighborID () == neighborID) && (it->first->GetValidity () == eslr::VALID))
      {
        retNeighborEntry = it;
        retVal = true;
        break;
      }
    }

    if (!retVal)
    {
      NS_LOG_LOGIC ("Neighbor not available: " << int (neighborID));
      return retVal;        
    }

    return retVal;
  }

	bool 
  NeighborTable::FindValidNeighborForAddress (Ipv4Address address, NeighborI &retNeighborEntry)
  {
    bool retVal = false;
    NeighborI it;
    for (it = m_neighborTable.begin ();  it!= m_neighborTable.end (); it++)
    {
      if ((it->first->GetNeighborAddress () == address) && (it->first->GetValidity () == eslr::VALID))
      {
        retNeighborEntry = it;
        retVal = true;
        break;
      }
    }
    if (!retVal)
    {
      NS_LOG_LOGIC ("Neighbor not available: " << address);
      return retVal;        
    }
    return retVal;   
  }

  void 
  NeighborTable::ReturnNeighborTable (NeighborTableInstance &instance)
  {
    for (NeighborI it = m_neighborTable.begin ();  it!= m_neighborTable.end (); it++)
    {
      instance.push_front(std::make_pair (it->first,it->second));
    }
  }

  void 
  NeighborTable::PrintNeighborTable (Ptr<OutputStreamWrapper> stream) const
  {
    std::ostream* os = stream->GetStream ();

    *os << "Neighbor ID  Neighbor Address    Local Interface  Auth Type     Validity" << '\n';
    *os << "-----------  ----------------    ---------------  ---------     --------" << '\n';
    for (NeighborCI it = m_neighborTable.begin ();  it!= m_neighborTable.end (); it++)
    {
      NeighborTableEntry *neighborEntry = it->first;
      std::ostringstream nw, authType, validity;
      // ID
      *os << std::setiosflags (std::ios::left) << std::setw (13) << neighborEntry->GetNeighborID ();
      // Address
      nw << neighborEntry->GetNeighborAddress () << "/" << int (neighborEntry->GetNeighborMask ().GetPrefixLength ());
      *os << std::setiosflags (std::ios::left) << std::setw (20) << nw.str ();
      // Local Interface
      *os << std::setiosflags (std::ios::left) << std::setw (17) << neighborEntry->GetInterface ();
      // Authentication Type
      if (neighborEntry->GetAuthType () == PLAIN_TEXT)
        authType << "Plain text";
      else if (neighborEntry->GetAuthType () == MD5)
        authType << "MD5";
      else if (neighborEntry->GetAuthType () == SHA)
        authType << "SHA";
      *os << std::setiosflags (std::ios::left) << std::setw (14) << authType.str ();
	
			// Validity of the neighbor record
      if (neighborEntry->GetValidity () == eslr::VALID)
        validity << "Valid";
      else if (neighborEntry->GetValidity () == eslr::INVALID)
        validity << "Invalid";
      *os << std::setiosflags (std::ios::left) << std::setw (7) << validity.str ();
      *os << '\n';
    }
    *os << "------------------------------------------------------------------------" << '\n';
  }

  std::ostream& operator<< (std::ostream& os, NeighborTableEntry const& neighbor)
  {
    os << static_cast<const NeighborTableEntry &>(neighbor);
    os << ", NeighborID: " << int (neighbor.GetNeighborID ()) << ",Interface: " << int (neighbor.GetInterface ());

    return os;
  }
}// end of namespace eslr
}// end of namespace ns3
