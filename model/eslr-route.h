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
   * \param networkMask network mask of the given destination network
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
   * \param networkMask network mask of the given destination network
   * \param interface interface index
   */
  RoutingTableEntry (Ipv4Address network = Ipv4Address (), 
                          Ipv4Mask networkMask = Ipv4Mask (),
                          uint32_t interface = 0);

  /**
   * \brief Constructor for creating a host route 
	 * 				This is used whenever administrators wanted to add host routes
	 * 				Specially for server-router communication protocol
   * \param host server's IP address
   * \param interface connected interface
   */
  RoutingTableEntry (Ipv4Address host = Ipv4Address (),
                          uint32_t interface = 0);

  virtual ~RoutingTableEntry ();

  /**
  * \brief Get and Set Sequence Number of the route record
  * \param sequenceNumber the sequence number of the route record
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
  * the metric is the Avg. propagation time for the destination network
  * \param metric the metric of the route record
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
  * 	The state of a route is changed according to the connectivity of neighbors
	* 	interfaces and destination network. In addition, the routers are marked
	* 	as INVALID when the expiration timer expires. Routes mark as DISCONNECTED
	* 	when either local interface or gateway is not respond. Further, routes mark
	* 	as BROKEN when the destination network disconnected.  
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
  * \brief the route type of the route record
  * 	All routes are in the main table are marked as PRIMARY (m-route)
  * 	Backup routing table maintains two route records for each destination prefix
	* 	listed in the main table. 
	* 	1. The reference route (r-route), which is a reference route to m-route. 
	* 		 marked as PRIMARY.
	* 	2. The backup route (b-route), which is the next best cost path for a 
	* 	   destination listed in ain table. marked as SECONDARY.
  * 	b-route is used for quich route recovery. 
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
  eslr::RouteType m_routeType; //!< The route record's type (Primary or Secondary)
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
   * \brief Add route for a network prefix.
   * This method is used to add newly discovered routes.
	 * Initially, routes are added to the backup table unless otherwise specified. 
	 * Routes are moved to the main table, after the settling time expires. 
   * However, for the very first time a route is received for a destination, the
	 * route is added to the main table for fast route discovery.
	 * When a route is added to the main table, a reference route will also added
	 * to the backup routing table.
	 * Therefore, each route that the node is receiving has to deal with the routes
	 * available in the backup table. This is specifically implemented to prevent
	 * route oscillations and to improve the reliability.
   * 
   * \param routingTableEntry The routing table entry
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime garbage collection time
   * \param table indicate table type (main or backup)
   */
  void AddRoute (RoutingTableEntry *routingTableEntry, 
			Time invalidateTime, 
			Time deleteTime, 
			Time settingTime, 
			eslr::Table table);

  /**
   * \brief This method is used to move a route to the main table after 
	 * settling time expires.
   * In addition, this method is used to update an existing route record 
	 * in the main table based on the 
   *
   * \param routingTableEntry The routing table entry
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing table before it moves to the Main
   */
  void MoveToMain (RoutingTableEntry *routingTableEntry, 
			Time invalidateTime, 
			Time deleteTime, 
			Time settlingTime);

  /**
  * \brief Delete a route record.
  * \param routingTableEntry corresponding route record
  * \param table indicate table type (main or backup)
  * \returns true if success  
  */
  bool DeleteRoute (RoutingTableEntry *routingTableEntry, 
			eslr::Table table);

  /**
   * \brief Add host route for a destination network.
   * This method is used to add host routes: routes about local interfaces 
	 * and routers about servers.
   * If the route is about router's local interfaces, all timers are set to zero. 
	 * Such routes will directly
   * This method is only use to add route to the main table.
   * However, route about a servers will have to follow the same procedure: 
	 * wait at the backup table
   * until the settling time expires. 
   *
   * \param routingTableEntry The routing table entry
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing table 
	 * 										 before it moves to the Main
   * \param table indicate table type (main or backup)
   */
  void AddHostRoute (RoutingTableEntry *routingTableEntry, 
			Time invalidateTime, 
			Time deleteTime, 
			Time settingTime, 
			eslr::Table table);

  /**
  * \brief Invalidate a route record
  * The invalidate method is call when ever a route is marked as
	* 1. INVALID
	* 2. BROKEN
	* 3. DISCONNECTED
	* 
	* The method used to process each and every state is different.  
  * 
	* In the first case, if the expired route is m-route, 
	* the main route will be then replaced by using either primary or backup route 
	* in the backup table accordingly. If the backup route is stable and its metric is
	* shorter than that of the primary route, both main and primary routes will be 
	* replaced using the backup route. Note that the backup route will be removed 
	* from the backup routing table after this process. 
	* Otherwise, the main route will be updated according to the primary route.
  *
  * In the first case, if the expired route is in the b-route,
  * an event will be scheduled to remove the secondary route from the table. 
  * Note that, r-route is not invalidated separately. It is changed according to the
	* m-route, 
  * such routes will not be invalidated.
  *
  * In both second and third case, if the expired route is m-route, 
	* replace the route from b-route, if a b-route is available. Otherwise delete
	* the m-route and p-route.
  *
  * \param routingTableEntry corresponding route record
  * \param invalidateTime the invalidate time
  * \param deleteTime garbage collection delay  
  * \param settlingTime time route has to wait at the backup routing table before it moves to the Main
  * \param InvalidateType indicate the reason for invalidate the route (e.g, broken route, expired route)
  * \param table indicate table type (main or backup) 
  * \returns true if success  
  */
  bool InvalidateRoute (RoutingTableEntry *routingTableEntry, 
			invalidateParams param);

  /**
  * \brief Find a route  and return.
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \param gateway the gateway of the route
  * \param table indicate table type (main or backup)   
  * \returns true and the corresponding route record if success  
  */
  bool FindRouteRecord (Ipv4Address destination, 
			Ipv4Mask netMask, 
			Ipv4Address gateway, 
			RoutesI &retRoutingTableEntry, 
			eslr::Table table);

  /**
  * \brief Find a route and return
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \param table indicate table type (main or backup)   
  * \returns true and the corresponding route record if success  
  */
  bool FindRouteRecord (Ipv4Address destination, 
			Ipv4Mask netMask, 
			RoutesI &retRoutingTableEntry, 
			eslr::Table table);

  /**
  * \brief Find a VALID route and return.
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \param retRoutingTableEntry the found routing table entry
  * \param table indicate table type (main or backup)   
  * \returns true and the corresponding route record if success  
  */
  bool FindValidRouteRecord (Ipv4Address destination, 
			Ipv4Mask netMask, 
			RoutesI &retRoutingTableEntry, 
			eslr::Table table);

  /**
  * \brief Find a VALID route return.
  * \param route the route to be checked
  * \param netMask network mask of the destination
  * \param found returns true if a route found
  * \param table indicate table type (main or backup)   
  * \returns the Iterator for the route
  */
  RoutingTable::RoutesI FindGivenRouteRecord (RoutingTableEntry *route, 
			bool &found, 
			eslr::Table table);
  
  /**
  * \brief Find a VALID route for a specific destination and return.
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \param found returns true if a route found
  * \param table indicate table type (main or backup)   
  * \returns the Iterator for the route
  */
  RoutingTable::RoutesI FindValidRouteRecordForDestination (Ipv4Address destination,
		 	Ipv4Mask netMask, 
			Ipv4Address gateway, 
			bool &found, 
			eslr::Table table);

  /**
  * \brief Find a VALID b-route for the given gateway and return.
  * \param destination find for the destination
  * \param netMask network mask of the destination  
  * \param routeType the routes type: PRIMARY, SECONDARY
  * \returns true and the corresponding route record if success  
  */
  bool FindRouteInBackup (Ipv4Address destination, 
			Ipv4Mask netMask, 
			RoutesI &retRoutingTableEntry, 
			eslr::RouteType routeType);
  
  /**
  * \brief Find a VALID b-route for the given destination and return.
  * \param destination find for the destination
  * \param netMask network mask of the destination 
  * \param found returns if found 
  * \param routeType the routes type: PRIMARY, SECONDARY
  * \returns the route record if found 
  */  
  RoutingTable::RoutesI FindRouteInBackupForDestination (Ipv4Address destination, 
			Ipv4Mask netMask, 
			bool &found, 
			eslr::RouteType routeType);
  
  /**
  * \brief check and return if local routes in the Main routing table.
  * \param destination find for the destination
  * \param netMask network mask of the destination
  * \returns true if local route found for the given network  
  */
  bool IsLocalRouteAvailable (Ipv4Address destination, 
			Ipv4Mask netMask);

  /**
   * \brief update a route.
   * This method is used to update routes in both main and backup table.
   * After processing of every RUM in the route advertisement message, 
	 * this method is called to update the relevant route record.
   *
   * In case of the route record is the main route in the main table, 
   * first ESLR checks any better and stable secondary route is available in 
	 * the backup table. If a secondary route found, both main and the corresponding 
	 * eslr::PRIMARY record in the backup table are update based on the secondary 
	 * route. Otherwise, the main route will be updated according to the 
	 * routingTableEntry.
   *
   * However, in case of the route record is in the backup table, the route record 
	 * is updated according to the routingTableEntry and the settling/expiration time
   * are set accordingly.  
   * 
   * \param routingTableEntry The routing table entry
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing tbale before it moves to the Main
   * \param table indicate table type (main or backup)
   */
  bool UpdateNetworkRoute (RoutingTableEntry *routingTableEntry, 
			Time invalidateTime, 
			Time deleteTime, 
			Time settlingTime, 
			eslr::Table table);

  /**
   * \brief update a locally connected networks. 
   * 			this needed to update directly connected networks  
	 * 			(i.e., server's cost). To restrict modifications, this method is 
	 * 			only consider metric updates.Note that, irrespective of the value 
	 * 			of the metric, the record will be updated. 
	 * 			
   * \param destination the server's IP address
   * \param netMask the network mask of the server
   * \param metric the server's cost
   */
  void UpdateLocalRoute (Ipv4Address destination, 
			Ipv4Mask netMask, 
			uint32_t metric);

  /**
  * \brief Return an instance of the routing table. 
	* 		This is specifically implemented for debug purposes.
	* 		Whenever someone get a instance of the table, it is his responsibility
	* 		to clear the instance for proper memory management.
	* 		
  * \param instance an instance of the std::list <RoutingTableEntry*, EventId>
  * \param table indicate table type (main or backup)   
  * \returns true if success  
  */
  void ReturnRoutingTable (RoutingTableInstance &instance, 
			eslr::Table table);

	/**
	 * \brief Get route records without any backup routes and return them.
	 * \param interface the interface which route records matched for
	 * \param instance the found route are returned as a route table instance
	 * */
	bool RoutesWithNoBackupRoutes (uint32_t interface, 
			RoutingTableInstance &instance); 

  /**
  * \brief Print the routing table
  * \param the output stream
  * \param table indicate table type (main or backup) 
  */
  void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, 
			eslr::Table table) const;

  /**
   * \brief Invalidate the routes related to one neighbor.
   * \param gateway the neighbor address
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing table 
	 * 				before it moves to the Main
   * \param table the table to be referred to find the routes.   
   */
  void InvalidateRoutesForGateway (Ipv4Address gateway, 
			Time invalidateTime, 
			Time deleteTime, 
			Time settlingTime, 
			eslr::Table table);

  /**
   * \brief Invalidate the routes related to one interface.
   * \param interface the interface address
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing table 
	 * 				before it moves to the Main
   * \param table the table to be referred to find the routes.
   */
  void InvalidateRoutesForInterface (uint32_t interface, 
			Time invalidateTime, 
			Time deleteTime, 
			Time settlingTime, 
			eslr::Table table);

  /**
   * \brief Invalidate the routes .
   * 				this method is called by the main implementation of the ESLR. 
   * \param destAddress the destination address
   * \param destMask the destination's network mask
   * \param gateway gateway that route should use to reach to the destination
   * \param invalidateTime the invalidate time
   * \param deleteTime garbage collection time
   * \param settlingTime time route has to wait at the backup routing table 
	 * 				before it moves to the Main
   * \param table indicate table type (main or backup) 
   * \returns true if route is found and invalidated.
   */
  bool InvalidateBrokenRoute (Ipv4Address destAddress, 
			Ipv4Mask destMask, 
			Ipv4Address gateway, 
			Time invalidateTime, 
			Time deleteTime, 
			Time settlingTime, 
			eslr::Table table);

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
  * \brief return a route to the given destination and the given net-device.
	* 			This method is used to forward the data packet.
  * \param destination destination network
  * \param dev the reference net-device
  * \param retRoutingTableEntry the returning routing table entry
  * \returns true if a route found for the specified destination address
  */
  bool ReturnRoute (Ipv4Address destination, 
			Ptr<NetDevice> dev, 
			RoutesI &retRoutingTableEntry); 

	/**
	 * \brief Dispose the routing module*/
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
