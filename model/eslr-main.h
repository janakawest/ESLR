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
#ifndef ESLR_MAIN
#define ESLR_MAIN

#include <list>
#include <map>

#include "eslr-definition.h"
#include "eslr-neighbor.h"
#include "eslr-headers.h"
#include "eslr-route.h"

#include "ns3/node.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/inet-socket-address.h"

#define ESLR_BROAD_PORT 275
#define ESLR_MULT_PORT 276
#define ESLR_MULT_ADD "224.0.0.250"

namespace ns3 {
namespace eslr {

class EslrRoutingProtocol : public Ipv4RoutingProtocol
{
public:

  EslrRoutingProtocol ();
  virtual ~EslrRoutingProtocol ();

  /**
   * \brief Get the type ID
   * \return type ID
   */
  static TypeId GetTypeId (void);

  // \name From Ipv4RoutingProtocol
  // \{
  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                              Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                   UnicastForwardCallback ucb, MulticastForwardCallback mcb, 
                   LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const;
  // \}

  /**
  * Assign a fixed random variable stream number to the random variables
  * used by this model.  Return the number of streams (possibly zero) that
  * have been assigned.
  *
  * \param stream first stream index to use
  * \return the number of stream indices assigned by this model
  */
  int64_t AssignStreams (int64_t stream);

  /**
   * \brief Get the set of interface excluded from the protocol
   * \return the set of excluded interfaces
   */
  std::set<uint32_t> GetInterfaceExclusions () const;

  /**
   * \brief Set the set of interface excluded from the protocol
   * \param exceptions the set of excluded interfaces
   */
  void SetInterfaceExclusions (std::set<uint32_t> exceptions);

  /**
   * \brief Add a default route to the router through the next-hop located on interface.
   *
   * The default route is usually installed manually, or it is the result of
   * some "other" routing protocol (e.g., BGP).
   *
   * \param nextHop the next hop
   * \param interface the interface
   */
  void AddDefaultRouteTo (Ipv4Address nextHop, uint32_t interface);

protected:
  /**
   * \brief Dispose this object.
   */
  virtual void DoDispose ();

  /**
   * Start protocol operation
   */
  void DoInitialize ();

private:
  /**
   * \brief Receive ESLR packets.
   *
   * \param socket the socket the packet was received to.
   */
  void Receive (Ptr<Socket> socket);
	/**
	 * \brief Handle the fast triggered update messages about the broken interfaces
	 * this method consideres Splithorison using two methods,
	 * 1. do not send route updates to the interface the node received fast triggered update
	 * 2. do not send route update to the actual gateway that the route is learned from
	 *
	 * \param hdr ESLRheader
	 * \pram senderAddress the address of the sender who sent the fast triggered update
	 * \param incominginterface is the interface node received the fast triggered update*/
	void HandleFastTrigUpdates (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface);
  /**
   * \brief Handle Keep Alive Messages
   *
   * \param hdr Keep Alive Message header (including KAMs)
   * \param senderAddress sender address
   * \param incomingInterface incoming interface
   */
  void HandleKamRequests (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface);
  
  /**
   * \brief Handle server advertisement messages
   *
   * \param hdr message header (including SRCs)
   * \param senderAddress sender address
   * \param incomingInterface incoming interface
   */  
  void HandleSrcRequests (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface);

	/**
	 * \brief Send a hello message at the begening*/
	void SendHelloMessage ();	

	/**
		* \brief send a hello message for a selected interface 
		*	\param interface the selected interface*/
	void SendHelloMessageForInterface (uint32_t interface);
 
  /**
   * \brief Send Keep alive Messages.  
   */
  void sendKams (); 

  /**
   * \brief Handle ESLR route request messages
   * \param hdr message header (including RUMs)
   * \param senderAddress sender address
   * \param senderPort sender port
   * \param incomingInterface incoming interface
   */
  void HandleRouteRequests (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint16_t senderPort, uint32_t incomingInterface);

  /**
   * \brief Handle ESLR route response messages
   * \param hdr message header (including RUMs)
   * \param senderAddress sender address
   * \param incomingInterface incoming interface
   */
  void HandleRouteResponses (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface);

	/**
	 * \brief Generate a unique ID for the node. 
	 * 	This ID is a hash value of (IF0's IP address + netmask + AS#) 
	 */ 

	uint32_t genarateNeighborID ();

	/**
	 * \brief Find the socket details for the incoming Interface. 
	 * \param interface the incoming interface 
   * \return the bounded socket
	 */
  Ptr<Socket> GetSocketForInterface (uint32_t interface);

	/**
	 * \brief Find the interface which a socket bounded to . 
	 * \param socket the socket
   * \return the bounded interface  
	 */
  int32_t GetInterfaceForSocket (Ptr<Socket> socket);

  /**
   * \brief Add route to network where the gateway address is known.
   * \param network network address
   * \param networkMask network prefix
   * \param nextHop next hop address to route the packet.
   * \param interface interface index
   * \param metric the cumulative propagation time to the destination network
   * \param sequenceNo sequence number of the received route
   * \param routeType is Route is Primary or Secondary
   * \param table where to add
   * \param timeoutTime time that the route is going to expire
   * \param grabageCollectionTime, time that the route is removed from the table
   * \param settlingTime time that a route has to wait before it is marked as valid route
   */
  void AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, Ipv4Address nextHop, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime);

  /**
   * \brief Add route to network where the gateway is not needed. Such routes are usefull to add
   * routes about the locally connected networks.
   * \param network network address
   * \param networkMask network prefix
   * \param interface interface index
   * \param metric the cumulative propagation time to the destination network
   * \param sequenceNo sequence number of the received route
   * \param routeType is Route is Primary or Secondary
   * \param table where to add
   * \param timeoutTime time that the route is going to expire
   * \param grabageCollectionTime, time that the route is removed from the table
   * \param settlingTime time that a route has to wait before it is marked as valid route
   */
  void AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime);

  /**
   * \brief Add route to a host.
   * \param host host address
   * \param interface interface index
   * \param metric the cumulative propagation time to the destination network
   * \param sequenceNo sequence number of the received route
   * \param routeType is Route is Primary or Secondary
   * \param table where to add
   * \param timeoutTime time that the route is going to expire
   * \param grabageCollectionTime, time that the route is removed from the table
   * \param settlingTime time that a route has to wait before it is marked as valid route
   */
  void AddHostRouteTo (Ipv4Address host, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime);

  /**
  * \brief invalidate all routes for a given interface
  * \param interface the reference interface
  * \param table routing table instance 
  */
  void InvalidateRoutesForInterface (uint32_t interface, eslr::Table table);

  /**
  * \brief Invalidate routes 
  * \param destAddres Address of the destination
  * \param destMask network mask of the destination
  * \param gateway gateway for the destination
  * \param table routing table instance 
  * \return true if found and invalidated
  */
  bool InvalidateBrokenRoute (Ipv4Address destAddress, Ipv4Mask destMask, Ipv4Address gateway, eslr::Table table);

  /**
   * \brief Add route to network.
   * \param network network address
   * \param networkMask network prefix
   * \param nextHop next hop address to route the packet.
   * \param interface interface index
   */
  void UpdateRoute (Ipv4Address network, Ipv4Mask networkMask, Ipv4Address nextHop, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime);

  /**
   * \brief Send Routing Updates for all neighbors.
   */
  void DoSendRouteUpdate (eslr::UpdateType updateType);

  /**
   * \brief Send Triggered Routing Updates on all interfaces.
   */
  void SendTriggeredRouteUpdate ();

  /**
   * \brief Send Unsolicited Routing Updates on all interfaces.
   */
  void SendPeriodicUpdate (void);
  
  /**
   * \brief calculate the cumulative cost of 
   * router's packet processing delay and links packet propagation delay.
   *
   * the route's packet processing delay (ts) is calculated based on
   * average packet arrival rate (i.e., Lambda) and 
   * average packet service rate (i.e., Mue)
   * the total packet processing cost for a route is 1/(Mue - Lambda) 
   * (Assumption: router is work based on the M/M/1 Queue model)
   * 
   * Link packet transmission delay (tr) is calculated based on the
   * link's capacity (i.e., link's bandwidth) (lc)
   * link's load (ll)(i.e., current occupancy of the link), which is calculated as follow.
   *  N1<------------>N2
   *  Get size (per second) to be transmitted of the N1's interface x1 = (#ofPacket * averagePacketSize * 8)
   *  Get size (per second) to be transmitted of the N2's interface x2 = (#ofPacket * averagePacketSize * 8)
   *  so that ll = x1 + x2
   *   Therefore available BW of the link (la) = lc - ll   
   * it results that tr = averagePacketSize / la
   *
   * In addition, Links have their own propagation delay (tp), which depends on the distance 
   * and the medium of the link. However, in this implementation, we added the link propagation delay 
   * as the delay we configured in the simulation script. 
   *
   * Consequently, a packet will take: ts + tp + tr delay to reach to it's next hop.
   *
   * \param dev the reference netdevice to retrieve link and interface attributes
   * \returns the LRCost of the node (time in micro Seconds)
   */  
  uint32_t CalculateLRCost (Ptr<NetDevice> dev);
  
  /**
   * \brief return the properties of the interface and its associated channel.
   * \param dev the reference netdevice to retrieve link and interface attributes
   * \param transDelay the links transaction delay (s)
   * \param propagationDelay the propagation delay link
   *
   *  the available bandwidth of the link is calculated as follow
   *
   * Link packet transmission delay (tr) is calculated based on the
   * link's capacity (i.e., link's bandwidth) (lc)
   * link's load (ll)(i.e., current occupancy of the link), which is calculated as follow.
   *  N1<------------>N2
   *  Get size (per second) to be transmitted of the N1's interface x1 = (#ofPacket * averagePacketSize * 8)
   *  Get size (per second) to be transmitted of the N2's interface x2 = (#ofPacket * averagePacketSize * 8)
   *  so that ll = x1 + x2
   *   Therefore available BW of the link (la) = lc - ll   
   */  
  void GetLinkDetails (Ptr<NetDevice> dev, double &transDelay, double &propagationDelay);
  
  /**
  * \brief look up for a forwarding route in the routing table.
  *
  * \param address destination address
  * \param dev output net-device if any (assigned 0 otherwise)
  * \return Ipv4Route where that the given packet has to be forwarded 
  */
  Ptr<Ipv4Route> LookupRoute (Ipv4Address address, Ptr<NetDevice> dev = 0); 
  
  /**
  * \brief the function developed for debugin purposes.
  * every m_printDuration the function will output number of protocol messages
  */
  void PrintStats ();

	/**
	 * \brief Send Route Pull messages amoung the neighbors for disconnected routers
	 * 				Find the routers which are invalidated because of the link disconnection
	 * 				Check whether any backup path is available or not
	 * 				if there is no backuppath send a RRQ message among the neighbors to get any available rouer
	 * 				
	 * \param interface the affected interface*/
	void PullRoutes (uint32_t interface);

// \name For prtocol management
// \{
// note: Since the result of socket->GetBoundNetDevice ()->GetIfIndex () is ambiguity and 
// it is dependent on the interface initialization (i.e., if the loop-back is already up) the socket
// list is used in this implementation. Sockets are then added to the relevant neighbor in the 
// neighbor table.
  /// Socket list type
  typedef std::map< Ptr<Socket>, uint32_t> SocketList;
  /// Socket list type iterator
  typedef std::map<Ptr<Socket>, uint32_t>::iterator SocketListI;
  /// Socket list type const iterator
  typedef std::map<Ptr<Socket>, uint32_t>::const_iterator SocketListCI;

  SocketList m_sendSocketList; //!< list of sockets for sending (socket, interface index)
  Ptr<Socket> m_recvSocket; //!< receive socket

  Ptr<Ipv4> m_ipv4; //!< IPv4 reference

  std::set<uint32_t> m_interfaceExclusions; //!< Set of excluded interfaces

  bool m_initialized; //!< flag to allow socket's late-creation.
  SplitHorizonType m_splitHorizonStrategy; //!< Split Horizon strategy
	PrintingOption m_print; //!< Printing Type

  Ptr<UniformRandomVariable> m_rng; //!< Rng stream.

  Time m_startupDelay; //!< Random delay before protocol start-up.

  EventId m_nextPeriodicUpdate; //!< Next periodic update event
  EventId m_nextTriggeredUpdate; //!< Next triggered update event
	uint8_t m_K1, m_K2, m_K3; //!< The CCV values
// \}

// \name for debugging
// \{
  EventId m_countingEvent; //!< Next statistic printing event
  uint64_t m_protocolMessages; //!< Number of protocol messages received between printing event
  Time m_printDuration; //!< Duration between two printing events
// \}

// \name For Neighbor management
// \{
  NeighborTable m_neighborTable; //!< the neighbor table
  NeighborTableEntry m_neighborEntry; //!< neighbor table entry
  Time m_kamTimer; //!< time between two keep alive messages 
  Time m_neighborTimeoutDelay; //!< Delay that determines the neighbor is UNRESPONSIVE
  Time m_garbageCollectionDelay; //!< Delay before remove UNRESPONSIVE route/neighbor record
  EventId m_nextKeepAliveMessage; //!< next Keep Alive Message event
	uint32_t m_nodeId; //!< the unique ID of the node
// \}

// \name for Routing Tables
// \{
  RoutingTable m_routing; //!< the routing table instances (Main and Backup)
  Time m_routeTimeoutDelay; //!< Delay that determines the route is UNRESPONSIVE
  Time m_routeSettlingDelay; //!< Delay that determines a particular route is stable
  Time m_minTriggeredCooldownDelay; //!< minimum cool-down delay between two triggered updates
  Time m_maxTriggeredCooldownDelay; //!< maximum cool-down delay between two triggered updates
  Time m_periodicUpdateDelay; //!< delay between two periodic updates

  int64_t m_stream; //!< stream for the uniform random variable
// \}
};// end of the class EslrRoutingProtocol
}// end of namespace eslr
}// end of namespave ns3

#endif /* ESLR_H */

