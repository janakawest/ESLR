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

#ifndef ESLR_ROUTING_H
#define ESLR_ROUTING_H

#include <cassert>
#include <list>
#include <sys/types.h>

#include "ns3/eslr-definition.h"
#include "ns3/eslr-headers.h"

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"

#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-route.h"
#include "ns3/timer.h"
#include "ns3/net-device.h"
#include "ns3/output-stream-wrapper.h"

namespace ns3 {
namespace eslr {

/**
 * \brief ESLR Routing Table Entry
 */
class RoutingTableEntry : public Ipv4RoutingTableEntry
{
public:

  RoutingTableEntry (void);

  /**
   * \brief Constructor
   * \param network network address
   * \param networkMask network mask of the given destination netowork
   * \param nextHop next hop address to route the packet
   * \param interface interface index
   */
  RoutingTableEntry (Ipv4Address network = Ipv4Address (), 
                          Ipv4Mask networkMask = Ipv4Mask (), 
                          Ipv4Address nextHop = Ipv4Address (), 
                          uint32_t interface = 0);

  /**
   * \brief Constructor
   * \param network network address
   * \param networkMask network mask of the given destination netowork
   * \param interface interface index
   */
  RoutingTableEntry (Ipv4Address network = Ipv4Address (), 
                          Ipv4Mask networkMask = Ipv4Mask (),
                          uint32_t interface = 0);

  /**
   * \brief Constructor for creating a host route (This is used for server-router communication protocol)
   * \param host server's IP address
   * \param interface connected interface
   */
  RoutingTableEntry (Ipv4Address host = Ipv4Address (),
                          uint32_t interface = 0);

  virtual ~RoutingTableEntry ();

  /**
  * \brief Get and Set Sequence Number of the route record
  * \param sequenceNumber the sequence number of the reute record
  * \returns the sequence number of the route record
  */

  uint16_t GetSequenceNo (void) const
  {
    return m_sequenceNo;
  }
  void SetSequenceNo (uint16_t sequenceNo)
  {
    m_sequenceNo = sequenceNo;
  }

  /**
  * \brief Get and Set metric 
  * the metric is the Avg. propagation time for the destination netowrk
  * \param metric the metric of the reute record
  * \returns the sequence number of the route record
  */

  uint32_t GetMetric (void) const
  {
    return m_metric;
  }
  void SetMetric (uint32_t metric)
  {
    m_metric = metric;
  }

  /**
  * \brief Get and Set route's status
  * The changed routes are scheduled for a Triggered Update.
  * After a Triggered Update, all the changed flags are cleared
  * from the routing table.
  *
  * \param changed true if the route is changed
  * \returns true if the route is changed
  */

  bool GetRouteChanged (void) const
  {
    return m_changed;
  }
  void SetRouteChanged (bool changed)
  {
    m_changed = changed;
  }

  /**
  * \brief Get and Set the route's status 
  * the route's validity is changed according to the expirationtime.
  * all new routes are first added to the backup table with status eslr::VALID
  * routes are then moved to the main table according to the cost and intially those are marked 
  * as eslr::VALID routes are set as eslr::INVALID after expiration time expires
  * unless the particular route record is updated or replaced by another best route
  * all eslr::INVALID routes are deleted after the garbage collection time
  *
  * \param validity the status of the route
  * \returns the status of the route record
  */
  eslr::Validity GetValidity () const
  {
    return eslr::Validity (m_validity);
  }
  void SetValidity (eslr::Validity validity)
  {
    m_validity = validity;
  }

  /**
  * \brief the route type of the route tecord
  * all routes are in the main table indicate that those are eslr::PRIMARY
  * as the backup routing table keeps two route records for a single destination, 
  * one record will indicate that it is a eslr::PRIMARY record. That record is the record in the main table
  * next record will indicate that it is a eslr::SECONDARY, which is the backup route.
  * In case the main route invalidated, the backup route will move to the main table by setting eslr::VALID
  * and eslr::PRIMARY. 
  * 
  * \param routeType type of the route
  * \returns the type of the route record
  */
  eslr::RouteType GetRouteType () const
  {
    return eslr::RouteType (m_routeType);
  }
  void SetRouteType (eslr::RouteType routeType)
  {
    m_routeType = routeType;
  }

  /**
  * \brief Get and route tag 
  * \param tag route tag
  * \returns the tag of the route
  */

  uint16_t GetRouteTag (void) const
  {
    return m_routeTag;
  }
  void SetRouteTag (uint16_t routeTag)
  {
    m_routeTag = routeTag;
  }

private:
  uint16_t m_sequenceNo; //!< sequence number of the route record
  uint32_t m_metric; //!< route metric
  bool m_changed; //!< route has been updated
  eslr::Validity m_validity; //!< validity of the routing record
  eslr::RouteType m_routeType; //!< The route record's type (Primary or Secondery)
  uint16_t m_routeTag; //!< route tag

};// end of RouteTableEntry


/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param route the Ipv4 routing table entry
 * \returns the reference to the output stream
 */
std::ostream& operator<< (std::ostream& os, RoutingTableEntry const& rte);

class RoutingTable
{
public:

  /// Container for a Route table entry 
  typedef std::pair <RoutingTableEntry*, EventId> RouteTableRecord;
  
  /// Container for an instance of the neighbor table
  typedef std::list<std::pair <RoutingTableEntry*, EventId> > RoutingTableInstance;

  /// Iterator for the Route table entry container
  typedef std::list<std::pair <RoutingTableEntry*, EventId> >::iterator RoutesI;

  /// Constant Iterator for the Route table entry container
  typedef std::list<std::pair <RoutingTableEntry*, EventId> >::const_iterator RoutesCI;

  /// Parameters that are needed to call the InvalidateRoutes method
  struct invalidateParams {
    Time invalidateTime;
    Time deleteTime;
    Time settlingTime;
    eslr::InvalidateType invalidateType;
    eslr::Table table;
  }; 

  RoutingTable();
  ~RoutingTable ();

  /**
   * \brief Add route for network to the main table.
   * this method is used to add the reoute records that received to the router.
   * first routes will be added to the backup table unless otherwise spesified.
   * then after settling time expires, this method will again called by the backup table to add the route
   * to the main table 
   * \param routingTableEntry The routing table entry
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime garbage collection time
   * \param table indicate table type (main or backup)
   */
  void AddRoute (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settingTime, eslr::Table table);

  /**
   * \brief Move a route to the main table after settling time expires.
   * then the corresponding route update will be updated as a reference to the main table
   * \param routingTableEntry The routing table entry
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing tbale before it moves to the Main
   */
  void MoveToMain (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settlingTime);

  /**
  * \brief Delete a routing record from the main routing table at the garbage collention
  * \param routingTableEntry currosponding route record
  * \param table indicate table type (main or backup)
  * \returns true if success  
  */
  bool DeleteRoute (RoutingTableEntry *routingTableEntry, eslr::Table table);

  /**
   * \brief Add host route for network to the main table.
   * This method is used to add host routes: routes aboout local interfaces, and routers abour servers
   * if the routes is about router's local interfaces, all timers are set to zeor. Such routes will directly
   * add to the main table. They are not expirring.
   * in case of a route aout a server, such routes are added to the backup table and execute the same 
   * sequence executed in the AddRoute method.
   * \param routingTableEntry The routing table entry
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing tbale before it moves to the Main
   * \param table indicate table type (main or backup)
   */
  void AddHostRoute (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settingTime, eslr::Table table);

  /**
  * \brief Invalidate a route record from the main table after Timeout seconds exceeedes
  * \param routingTableEntry currosponding route record
  * \param invalidateTime the invalidate time
  * \param deleteTime garbage collection delay  
  * \param settlingTime time route has to wait at the backup routing tbale before it moves to the Main
  * \param InvalidateType the parameter indicate the reason for invalidate the route (e.g, broken route, expired route)
  * \param table indicate table type (main or backup) 
  * \returns true if success  
  */
  //bool InvalidateRoute (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settlingTime, eslr::InvalidateType invalidateType, eslr::Table table);
  bool InvalidateRoute (RoutingTableEntry *routingTableEntry, invalidateParams param);

  /**
  * \brief Find a  route in the main table and return it if exist. Do not consider the VALID flag
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \param gateway the gateway of the route
  * \param table indicate table type (main or backup)   
  * \returns true and the currosponding route record if success  
  */
  bool FindRouteRecord (Ipv4Address destination, Ipv4Mask netMask, Ipv4Address gateway, RoutesI &retRoutingTableEntry, eslr::Table table);

  /**
  * \brief Find a  route in the main table and return it if exist. Do not consider the VALID flag
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \param table indicate table type (main or backup)   
  * \returns true and the currosponding route record if success  
  */
  bool FindRouteRecord (Ipv4Address destination, Ipv4Mask netMask, RoutesI &retRoutingTableEntry, eslr::Table table);

  /**
  * \brief Find a VALID route in the main table and return it if exist.
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \param gateway the gateway of the route
  * \param table indicate table type (main or backup)   
  * \returns true and the currosponding route record if success  
  */
  bool FindValidRouteRecord (Ipv4Address destination, Ipv4Mask netMask, Ipv4Address gateway, RoutesI &retRoutingTableEntry, eslr::Table table);

  /**
  * \brief Find a VALID route in the main table and return it if exist.
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \param table indicate table type (main or backup)   
  * \returns true and the currosponding route record if success  
  */
  bool FindValidRouteRecord (Ipv4Address destination, Ipv4Mask netMask, RoutesI &retRoutingTableEntry, eslr::Table table);
  RoutingTable::RoutesI FindValidRouteRecord_1 (Ipv4Address destination, Ipv4Mask netMask, Ipv4Address gateway, bool &found, eslr::Table table);
  /**
  * \brief Find a route for the given interface in the main table and return it if exist.
  * \param interface interface that referes
  * \param matchedRouteEntries matched route records
  * \param table indicate table type (main or backup)   
  * \returns true and the currosponding route record if success  
  */
  bool FindRoutesForInterface (uint32_t interface, RoutingTable::RoutingTableInstance &matchedRouteEntries, eslr::Table table);

  /**
  * \brief Find a VALID route for the given gateway in the main table and return it if exist.
  * \param interface interface that referes
  * \param matchedRouteEntries matched route records
  * \param table indicate table type (main or backup)   
  * \returns true and the currosponding route record if success  
  */
  bool FindRoutesForGateway (Ipv4Address gateway, RoutingTable::RoutingTableInstance &matchedRouteEntries, eslr::Table table);

  /**
  * \brief Find a VALID secondary (backup )route for the given gateway in the backup table and return.
  * \param destination find for the destination
  * \param netMask network mask of the destination  
  * \param routeType the routes type: PRIMARY, SECONDARY
  * \returns true and the currosponding route record if success  
  */
  bool FindRouteInBackup (Ipv4Address destination, Ipv4Mask netMask, RoutesI &retRoutingTableEntry, eslr::RouteType routeType);
  RoutingTable::RoutesI FindRouteInBackup_1 (Ipv4Address destination, Ipv4Mask netMask, bool &found, eslr::RouteType routeType);
  /**
  * \brief check and return if local route presents in the Main routing table.
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \returns true if local route found for the given network  
  */
  bool IsLocalRouteAvailable (Ipv4Address destination, Ipv4Mask netMask);

  /**
   * \brief update a route to network to the main table.
   * this method is used to add the reoute records that received to the router
   * \param routingTableEntry The routing table entry
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing tbale before it moves to the Main
   * \param table indicate table type (main or backup)
   */
  bool UpdateNetworkRoute (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table);

  /**
  * \brief Return an instance of the Main routing table. This is specifically implemented for debug purposes
  * \param instance an instance of the std::list <RoutingTableEntry*, EventId>
  * \param table indicate table type (main or backup)   
  * \returns true if success  
  */
  void ReturnRoutingTable (RoutingTableInstance &instance, eslr::Table table);

  /**
  * \brief Print the routing table
  * \param the output stream
  * \param table indicate table type (main or backup) 
  */
  void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, eslr::Table table) const;

  /**
   * \brief Invalidate the routes realated to one neighbor.
   * this method is called by the neighbor module when an neighbor recored is invalidated
   * \param gateway the neighbor address
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing tbale before it moves to the Main
   * \param table the table to be reffered to find the routes.   
   */
  void InvalidateRoutesForGateway (Ipv4Address gateway,  Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table);

  /**
   * \brief Invalidate the routes realated to one interface.
   * this method is called by the neighbor module when an neighbor recored is invalidated
   * \param interface the interface address
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing tbale before it moves to the Main
   * \param table the table to be reffered to find the routes.
   */
  void InvalidateRoutesForInterface (uint32_t interface,  Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table);

  /**
   * \brief Invalidate the routes .
   * this method is called by the main implementation of the ESLR. 
   * \param destAddress the destination address
   * \param destMask the destination's network mask
   * \param gateway gateway that route shoud use to reach to the destination
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing tbale before it moves to the Main
   * \param table indicate table type (main or backup) 
   * \returns true if route is found and invalidated.
   */
  bool InvalidateBrokenRoute (Ipv4Address destAddress, Ipv4Mask destMask, Ipv4Address gateway, Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table);

  /**
  * \brief return if the routing table is empty or not
  * \param table indicate table type (main or backup)
  */
  bool IsEmpty(eslr::Table table);

  /**
  * \brief increment the sequence number of locally connected routes
  */
  void IncrementSeqNo ();
  
  /**
  * \brief toggle changed flag of all routes
  */
  void ToggleRouteChanged ();
  
  /**
  * \brief return a route to the given destination and the given netdevice
  * \param destination destination network
  * \param dev the reference netdevice
  * \param retRoutingTableEntry the returning routing table entry
  * \returns true if a route found for the specified destination address
  */
  bool ReturnRoute (Ipv4Address destination, Ptr<NetDevice> dev, RoutesI &retRoutingTableEntry); 
  
  /**
  * \brief delete the secondary route in the backup database
  * \param destination destination network
  * \param mask mask of the destination
  */  
  void DeleteSecondaryRoute (Ipv4Address destination, Ipv4Mask mask);

	void DoDispose ()
	{
		m_mainRoutingTable.clear ();
    m_backupRoutingTable.clear ();
	}

  /**
  * \brief assign a stream to Uniform Random Variable
  */
  void AssignStream (int64_t stream);

	/**
	 * \brief assign the IPv4 pointer to routing management
	 * \param ipv4 the IPv4 pointer
	 */
	void AssignIpv4(Ptr<Ipv4> ipv4)
	{
		m_ipv4 = ipv4;
		m_nodeId = m_ipv4->GetObject<Node> ()->GetId ();
	}

private:
  RoutingTableInstance m_mainRoutingTable; //!< Instance of the Main Routing Table
  RoutingTableInstance m_backupRoutingTable; //!< Instance of the Backup Routing Table

  Ptr<UniformRandomVariable> m_rng; //!< Rng stream.
	Ptr<Ipv4> m_ipv4; //!< Ipv4 pointer
	Ptr<Node> m_node; //!< node the routing protocol is running on 
	uint32_t m_nodeId; //!< node id
};// end of RouteTable 
}// end of namespace eslr
}// end of namespace ns3
#endif /* ROUTING */
