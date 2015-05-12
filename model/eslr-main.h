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

//#include "ns3/output-stream-wrapper.h"
//#include "ns3/tcp-socket.h"

#define ESLR_BROAD_PORT 275
#define ESLR_MULT_PORT 276
#define ESLR_MULT_ADD "224.0.0.250"

namespace ns3 {
namespace eslr {

/**
 * Split Horizon strategy type.
 */
enum SplitHorizonType {
  NO_SPLIT_HORIZON,//!< No Split Horizon
  SPLIT_HORIZON,   //!< Split Horizon
};

/**
 * Printing Options.
 */
enum PrintingOption {
  MAIN_R_TABLE, //!< Print the main routing table
  BACKUP_R_TABLE, //!< Print the backup routing table
  N_TABLE, //!< Print the neighbor table
};

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
   * \brief Add a default route to the router through the nextHop located on interface.
   *
   * The default route is usually installed manually, or it is the result of
   * some "other" routing protocol (e.g., BGP).
   *
   * \param nextHop the next hop
   * \param interface the interface
   */
  //void AddDefaultRouteTo (Ipv4Address nextHop, uint32_t interface);

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
   * \brief Handle Keep Alive Messages
   *
   * \param kam Keep Alive Message header (including KAMs)
   * \param senderAddress sender address
   * \param incomingInterface incoming interface
   */
  void HandleKamRequests (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface);

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
	 * \brief Genarate a unique ID for the node. 
	 * 				This ID is a hash value of (IF0's IP address + netmask + AS#) 
	 */ 

	uint32_t genarateNeighborID ();

	/**
	 * \brief Find the soceket details for the incomming Interface. 
	 * \param interface the incomming interface 
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
   * \param metric the cumilative propagation time to the destination network
   * \param sequenceNo sequence number of the received route
   * \param routeType is Route is Primary or Secondary
   * \param table where to add
   * \param timeoutTime time that the route is going to expire
   * \param grabageCollectionTime, time that the route is removed from the table
   * \param settlingTime time that a route has to waid before it is stated as valid route
   */
  void AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, Ipv4Address nextHop, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime);

  /**
   * \brief Add route to network where the gateway is not needed. Such routes are usefull to add
   * routes about the localy connected networks.
   * \param network network address
   * \param networkMask network prefix
   * \param interface interface index
   * \param metric the cumilative propagation time to the destination network
   * \param sequenceNo sequence number of the received route
   * \param routeType is Route is Primary or Secondary
   * \param table where to add
   * \param timeoutTime time that the route is going to expire
   * \param grabageCollectionTime, time that the route is removed from the table
   * \param settlingTime time that a route has to waid before it is stated as valid route
   */
  void AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime);

  /**
   * \brief Add route to a host.
   * \param network network address
   * \param interface interface index
   * \param metric the cumilative propagation time to the destination network
   * \param sequenceNo sequence number of the received route
   * \param routeType is Route is Primary or Secondary
   * \param table where to add
   * \param timeoutTime time that the route is going to expire
   * \param grabageCollectionTime, time that the route is removed from the table
   * \param settlingTime time that a route has to waid before it is stated as valid route
   */
  void AddHostRouteTo (Ipv4Address host, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime);

  /**
  * \brief invalidate all routes match the given interface
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
   * \brief Send Routing Updates on all interfaces.
   */
  void DoSendRouteUpdate (eslr::UpdateType updateType);

  /**
   * \brief Send Routing Request on all interfaces.
   */
  void SendRouteRequest ();

  /**
   * \brief Send Triggered Routing Updates on all interfaces.
   */
  void SendTriggeredRouteUpdate ();

  /**
   * \brief Send Unsolicited Routing Updates on all interfaces.
   */
  void SendPeriodicUpdate (void);

// \name For prtocol management
// \{
// note: Since the result of socket->GetBoundNetDevice ()->GetIfIndex () is ambiguity and 
// it is dependent on the interface initialization (i.e., if the loopback is already up) the socket
// list is used inthis implementation. Sockets are then added to the relavent neighber in the 
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

  Time m_startupDelay; //!< Random delay before protocol startup.

  EventId m_nextPeriodicUpdate; //!< Next periodic update event
  EventId m_nextTriggeredUpdate; //!< Next triggered update event
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
  Time m_routeSettlingDelay; //!< Delay that determins a particular route is stable
  Time m_minTriggeredCooldownDelay; //!< minimum cooldown delay between two triggered updates
  Time m_maxTriggeredCooldownDelay; //!< maximum cooldown delay between two triggered updates
  Time m_periodicUpdateDelay; //!< delay between two periodic updates

  int64_t m_stream;
// \}
};// end of the class EslrRoutingProtocol
}// end of namespace eslr
}// end of namespave ns3

#endif /* ESLR_H */
