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
#include <string>

#include "eslr-main.h"

#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/assert.h"
#include "ns3/unused.h"
#include "ns3/random-variable-stream.h"
#include "ns3/node.h"
#include "ns3/names.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/node-list.h"
#include "ns3/timer.h"
#include "ns3/channel.h"
#include "ns3/ipv4-packet-info-tag.h"


NS_LOG_COMPONENT_DEFINE ("EslrRoutingProtocol");

namespace ns3 {
namespace eslr {
NS_OBJECT_ENSURE_REGISTERED (EslrRoutingProtocol);
EslrRoutingProtocol::EslrRoutingProtocol() :  m_ipv4 (0),
                                              m_initialized (false),
                                              m_protocolMessages (0),
																							m_neighborTable ()
{
  m_rng = CreateObject<UniformRandomVariable> ();
}

EslrRoutingProtocol::~EslrRoutingProtocol () {/*destructor*/}

TypeId EslrRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::eslr::EslrRoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .AddConstructor<EslrRoutingProtocol> ()
    .AddAttribute ( "KeepAliveInterval","The time between two Keep Alive Messages.",
			              TimeValue (Seconds(30)), /*This should adjust according to the user requirement*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_kamTimer),
			              MakeTimeChecker ())
    .AddAttribute ( "NeighborTimeoutDelay","The delay to mark a neighbor as unresponsive.",
			              TimeValue (Seconds(35)), /*This should adjust according to the user requirement*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_neighborTimeoutDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "GarbageCollection","The delay to remove unresponsive neighbors from the neighbor table.",
			              TimeValue (Seconds(10)), /*This should adjust according to the user requirement*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_garbageCollectionDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "StartupDelay", "Maximum random delay for protocol startup (send route requests).",
                    TimeValue (Seconds(1)),
                    MakeTimeAccessor (&EslrRoutingProtocol::m_startupDelay),
                    MakeTimeChecker ())
    .AddAttribute ( "SplitHorizon", "Split Horizon strategy.",
                    EnumValue (SPLIT_HORIZON),
                    MakeEnumAccessor (&EslrRoutingProtocol::m_splitHorizonStrategy),
                    MakeEnumChecker (NO_SPLIT_HORIZON, "NoSplitHorizon",
                                      SPLIT_HORIZON, "SplitHorizon"))
    .AddAttribute ( "RouteTimeoutDelay","The delay to mark a route is invalidate.",
			              TimeValue (Seconds(150)), /*This should adjust according to the user requirement*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_routeTimeoutDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "SettlingTime","The delay that a route record has to keep in the backup table before it is moved to the main table.",
			              TimeValue (Seconds(100)), /*This should adjust according to the user requirement*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_routeSettlingDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "MinTriggeredCooldown","Minimum time gap between two triggered updates.",
			              TimeValue (Seconds(1)), /*This should adjust according to the user requirement*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_minTriggeredCooldownDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "MaxTriggeredCooldown","Maximum time gap between two triggered updates.",
			              TimeValue (Seconds(5)), /*This should adjust according to the user requirement*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_maxTriggeredCooldownDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "PeriodicUpdateInterval","Time between two periodic updates.",
			              TimeValue (Seconds(50)), /*This should adjust according to the user requirement*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_periodicUpdateDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "PrintingMethod", "Specify which table is to print.",
                    EnumValue (DONT_PRINT),
                    MakeEnumAccessor (&EslrRoutingProtocol::m_print),
                    MakeEnumChecker ( MAIN_R_TABLE, "MainRoutingTable",
                                      N_TABLE, "NeighborTable",
                                      BACKUP_R_TABLE, "BackupRoutingTable"))
    .AddAttribute ( "DebugPrintingDuration", "Time gap between two debug messages.",
                    TimeValue (Seconds(20)),
                    MakeTimeAccessor (&EslrRoutingProtocol::m_printDuration),
                    MakeTimeChecker ())
		.AddAttribute ( "K1", "The value of the CCV K1 for Servers.",
			 							UintegerValue (1),
										MakeUintegerAccessor (&EslrRoutingProtocol::m_K1),
										MakeUintegerChecker<uint8_t> ())
		.AddAttribute ( "K2", "The value of the CCV K2 Links.",
			 							UintegerValue (1),
										MakeUintegerAccessor (&EslrRoutingProtocol::m_K2),
										MakeUintegerChecker<uint8_t> ())
		.AddAttribute ( "K3", "The value of the CCV K3 Routers.",
			 							UintegerValue (1),
										MakeUintegerAccessor (&EslrRoutingProtocol::m_K3),
										MakeUintegerChecker<uint8_t> ())

  ;
  return tid;
}

int64_t
EslrRoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  
  m_rng->SetStream (stream);
  m_stream = stream+5;

  return 1;
}

void 
EslrRoutingProtocol::DoInitialize ()
{
  NS_LOG_FUNCTION (this);

  bool addedGlobal = false;
  m_initialized = true; // Routing Protocol is initiated
	Time delay;

  m_routing.AssignStream (m_stream);
	m_routing.AssignIpv4 (m_ipv4);

  // build the socket and interface list
  // NOTE:
  // Because, 0th interface is always the loop back interface "127.0.0.1", 
  // for the route management, the interface 0 is Purposely omitted
  
  for (uint32_t interfaceId = 1 ; interfaceId < m_ipv4->GetNInterfaces (); interfaceId++)
  {
    bool activeInterface = false;
    if (m_interfaceExclusions.find (interfaceId) == m_interfaceExclusions.end ())
    {
      activeInterface = true;
    } 

    for (uint32_t intAdd = 0; intAdd < m_ipv4->GetNAddresses (interfaceId); intAdd++)
    {
      Ipv4InterfaceAddress iface = m_ipv4->GetAddress (interfaceId,intAdd);
      if (iface.GetScope() == Ipv4InterfaceAddress::GLOBAL && activeInterface == true)
      {
        NS_LOG_LOGIC ("ESLR: Adding sending socket to " << iface.GetLocal ());
    
        Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
        NS_ASSERT (socket != 0);

        socket->Bind (InetSocketAddress(iface.GetLocal (), ESLR_BROAD_PORT));
        socket->BindToNetDevice (m_ipv4->GetNetDevice (interfaceId));
        socket->SetAllowBroadcast (true);
       
        //socket->SetIpTtl (1);
        socket->SetIpRecvTtl (true);
        socket->SetRecvCallback (MakeCallback (&EslrRoutingProtocol::Receive,this));
        socket->SetRecvPktInfo (true);

        NS_LOG_LOGIC ("ESLR: add the socket to the socket list " << iface.GetLocal ());
        m_sendSocketList[socket] = interfaceId;
      	addedGlobal = true;
			}
    }
  }

  if (!m_recvSocket)
  {
    NS_LOG_LOGIC ("ESLR: Adding receiving socket");
      
    m_recvSocket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
    NS_ASSERT (m_recvSocket != 0);

    m_recvSocket->Bind (InetSocketAddress(Ipv4Address::GetAny (), ESLR_MULT_PORT));
    //m_recvSocket->SetIpTtl (1);
    m_recvSocket->SetIpRecvTtl (true);
    m_recvSocket->SetRecvCallback (MakeCallback (&EslrRoutingProtocol::Receive,this));
    m_recvSocket->SetRecvPktInfo (true);
  }

	NS_LOG_DEBUG ("ESLR: Broadcasting Hello Messages");
	SendHelloMessage ();

  // If there are newly added routes, schedule both triggered and periodic updates
  if (addedGlobal)
  {
    delay = Seconds (m_rng->GetValue (m_minTriggeredCooldownDelay.GetSeconds (),
                                      m_maxTriggeredCooldownDelay.GetSeconds ()));
    m_nextTriggeredUpdate = Simulator::Schedule (delay, &EslrRoutingProtocol::DoSendRouteUpdate, 
                                                 this, eslr::TRIGGERED);
  }

  // Otherwise schedule a periodic update
  delay = m_periodicUpdateDelay + Seconds (m_rng->GetValue (0, m_periodicUpdateDelay.GetSeconds ()));
  m_nextPeriodicUpdate = Simulator::Schedule (delay, &EslrRoutingProtocol::SendPeriodicUpdate, this);
  
//  // Initialize the periodic debug counter
//  m_countingEvent = Simulator::Schedule (m_printDuration, &EslrRoutingProtocol::PrintStats, this); 
    
  Ipv4RoutingProtocol::DoInitialize ();

	// pass an instance of the routing table to the neighbor management module.
	// FIXME: Still need some fixing
	m_neighborTable.DoInitialize (m_routing, m_routeTimeoutDelay, m_garbageCollectionDelay, m_routeSettlingDelay);
 
}
void 
EslrRoutingProtocol::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
  {
    iter->first->Close ();
  }
  m_sendSocketList.clear ();
  m_recvSocket->Close ();
  m_recvSocket = 0;

  m_nextKeepAliveMessage.Cancel ();
  m_nextKeepAliveMessage = EventId ();

  m_nextTriggeredUpdate.Cancel ();
  m_nextTriggeredUpdate = EventId ();

  m_nextPeriodicUpdate.Cancel ();
  m_nextPeriodicUpdate = EventId ();

  m_ipv4 = 0;

  m_neighborTable.DoDispose ();

  m_routing.DoDispose ();
}

void 
EslrRoutingProtocol::NotifyInterfaceUp (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);

  for (uint32_t i = 0; i < m_ipv4->GetNAddresses (interface); i++)
  {
    Ipv4InterfaceAddress iface = m_ipv4->GetAddress (interface,i);
  	Ipv4Mask ifaceNetMask = iface.GetMask();
    Ipv4Address ifaceNetworkAddress = iface.GetLocal().CombineMask(ifaceNetMask);  

    if (iface.GetLocal() != Ipv4Address () && ifaceNetMask != Ipv4Mask ())
    {
      if (iface.GetLocal() == Ipv4Address ("127.0.0.1"))
      {
        /* host route for the Interface 0*/
        AddHostRouteTo (iface.GetLocal(), 0, 0, 0, 
                        eslr::PRIMARY, eslr::MAIN, 
                        Seconds (0), Seconds (0), Seconds (0));
      }
      else
      {
        AddNetworkRouteTo (ifaceNetworkAddress, ifaceNetMask, interface, 0, 0, 
                           eslr::PRIMARY, eslr::MAIN, 
                           Seconds (0), Seconds (0), Seconds (0));
      }
    }    
  }

  // if protocol is not started yet, socket list will be created and modified later
  if (!m_initialized)
    return; 

  bool foundSendSocket = false;
  for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
  {
    if (iter->second == interface)
    {
      foundSendSocket = true;
      break;
    }
  }
  
  bool activeInterface = false;
  if (m_interfaceExclusions.find (interface) == m_interfaceExclusions.end ())
  {
    activeInterface = true;
  }
  
  for (uint32_t i = 0; i < m_ipv4->GetNAddresses (interface); i++)
  {
    Ipv4InterfaceAddress iface = m_ipv4->GetAddress (interface,i);
  	Ipv4Mask ifaceNetMask = iface.GetMask();
    Ipv4Address ifaceNetworkAddress = iface.GetLocal ().CombineMask (ifaceNetMask);  
    
    if (iface.GetScope () == Ipv4InterfaceAddress::GLOBAL && 
        foundSendSocket == false && 
        activeInterface == true)
    {
      NS_LOG_DEBUG ("ESLR: Adding sending socket to " << iface.GetLocal ());
      
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
      NS_ASSERT (socket != 0);

      socket->Bind (InetSocketAddress(iface.GetLocal (),ESLR_BROAD_PORT));
      socket->BindToNetDevice (m_ipv4->GetNetDevice (interface));

      //socket->SetIpTtl (1);
      socket->SetIpRecvTtl (true);
      socket->SetAllowBroadcast (true);
      socket->SetRecvCallback (MakeCallback (&EslrRoutingProtocol::Receive,this));
      socket->SetRecvPktInfo (true);

      NS_LOG_DEBUG ("ESLR: Add the socket to the socket list " << iface.GetLocal ());
      m_sendSocketList[socket] = interface;
			
			NS_LOG_DEBUG ("ESLR: Initiate the neighbor discovery process for " <<  interface);
			SendHelloMessageForInterface (interface);
    }
  }
  
  if (!m_recvSocket)
  {
    NS_LOG_LOGIC ("ESLR: Adding receiving socket");
      
    m_recvSocket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
    NS_ASSERT (m_recvSocket != 0);

    m_recvSocket->Bind (InetSocketAddress(Ipv4Address::GetAny (), ESLR_MULT_PORT));
    //m_recvSocket->SetIpTtl (1);
    m_recvSocket->SetIpRecvTtl (true);
    m_recvSocket->SetRecvCallback (MakeCallback (&EslrRoutingProtocol::Receive,this));
    m_recvSocket->SetRecvPktInfo (true);
  }

	// Notify about the recoverd interface and the updated route
	SendTriggeredRouteUpdate ();
}

void 
EslrRoutingProtocol::NotifyInterfaceDown (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
  
  // NOTE:
  // All routes that are referring this interface has to remove from both routing tables, 
  // Neighbors are not invalidated forcefully.
  
	// TODO
	// 1. --> For those who do not have backup paths, send RRQs among neighbors

	// Invalidate the Route records for broken interfaces
  InvalidateRoutesForInterface (interface, eslr::BACKUP);
  InvalidateRoutesForInterface (interface, eslr::MAIN);

	// Close down the local connection sockets and remove them.
	// Send a fast triggered update about the disconnected interface among the remaining neighbors
	for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
  {
    NS_LOG_LOGIC ("ESLR: Checking socket for interface " << interface);

    if (iter->second == interface)
    {
      NS_LOG_LOGIC ("ESLR: Remove socket for interface " << interface);

      iter->first->Close ();
      m_sendSocketList.erase (iter);
      break;
    }
	}

  // Get disconnected network
  Ipv4InterfaceAddress iface = m_ipv4->GetAddress (interface,0);
  Ipv4Mask ifaceNetMask = iface.GetMask();
  Ipv4Address ifaceNetworkAddress = iface.GetLocal().CombineMask(ifaceNetMask);
	
	// Acquiring an instance of the neighbor table
  NeighborTable::NeighborTableInstance tempNeighbor;
  m_neighborTable.ReturnNeighborTable (tempNeighbor);
  NeighborTable::NeighborI it;

  // Bypass the triggered update sequence and send a Fast triggered update message
  NS_LOG_DEBUG ("ESLR: Bypass the existing triggered hold-down");
  if (m_nextTriggeredUpdate.IsRunning ())
    m_nextTriggeredUpdate.Cancel ();  	
  
  // Create fast triggered update message
  Ptr<Packet> p = Create<Packet> ();
  SocketIpTtlTag tag;
  p->RemovePacketTag (tag);
  tag.SetTtl (0);
  p->AddPacketTag (tag);

  ESLRRoutingHeader hdr;
  hdr.SetCommand (eslr::RU);
  hdr.SetRuCommand (eslr::RESPONSE);
  hdr.SetRoutingTableRequestType (eslr::NON);
  hdr.SetFastTrigUpdate (true);
  hdr.SetDbit (true);

  ESLRrum rum;
  rum.SetSequenceNo (1); // reset the sequence number.
  rum.SetMatric (0); // Since no zero delay is available, zero is considered as disconnected.
  rum.SetDestAddress (ifaceNetworkAddress);
  rum.SetDestMask (ifaceNetMask);
	rum.SetDbit (true);

  NS_LOG_LOGIC ("ESLR: SendTo: " << *p);
        
	for (it = tempNeighbor.begin ();  it != tempNeighbor.end (); it++)
  {
		if (it->first->GetInterface () != interface)
		{
	    if (m_interfaceExclusions.find (it->first->GetInterface ()) == m_interfaceExclusions.end ())
	    {
				// Authentication is necessary
  			hdr.SetAuthType (it->first->GetAuthType ()); 
  			hdr.SetAuthData (it->first->GetAuthData ());
			  
				hdr.AddRum (rum);
  			p->AddHeader (hdr);

        // send it via link local broadcast
        NS_LOG_LOGIC ("ESLR: Send a fast triggered update to " << it->first->GetNeighborAddress ());
        Ipv4Address broadAddress = it->first->GetNeighborAddress ().GetSubnetDirectedBroadcast (it->first->GetNeighborMask ());
        it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_MULT_PORT));

				p->RemoveHeader (hdr);
				hdr.ClearRums ();
      }
    }
	}

  //Clear the temporary neighbor table instance
  tempNeighbor.clear ();

	// Send Route Pull request for the routers invalidated and does not have any backup routes.
	PullRoutes (interface);

	// Re schedule the triggered update
	NS_LOG_DEBUG ("ESLR: Reset the triggered hold-down");
	Time delay = Seconds (m_rng->GetValue (	m_minTriggeredCooldownDelay.GetSeconds (),
	                      									m_maxTriggeredCooldownDelay.GetSeconds ()));
  m_nextTriggeredUpdate = Simulator::Schedule (delay, &EslrRoutingProtocol::DoSendRouteUpdate,
                                           this, eslr::TRIGGERED);
}

void
EslrRoutingProtocol::PullRoutes (uint32_t interface)
{
	bool foundRoutes = false;
	RoutingTable::RoutingTableInstance routes;
	uint32_t dInterface = interface;
	
	foundRoutes = m_routing.RoutesWithNoBackupRoutes (dInterface, routes);

  // Acquiring an instance of the neighbor table
  NeighborTable::NeighborTableInstance tempNeighbor;
  m_neighborTable.ReturnNeighborTable (tempNeighbor);
  NeighborTable::NeighborI it;
	
	if (foundRoutes)
	{
    ESLRRoutingHeader hdr;	
    ESLRrum rum;

    for (it = tempNeighbor.begin ();  it != tempNeighbor.end (); it++)
    {
      if (it->first->GetInterface () == dInterface)
      {
        NS_LOG_LOGIC("ESLR: the disconnected interface is omitted");
        continue;
      }
      
      if (m_interfaceExclusions.find (it->first->GetInterface ()) == m_interfaceExclusions.end ())
      {
        // In case number of destination addresses exceeds the header size
        // Calculating the Number of RUMs that can add to the ESLR Routing Header
        uint16_t mtu = m_ipv4->GetMtu (it->first->GetInterface ());
        uint16_t maxRum = (mtu - 
                           Ipv4Header ().GetSerializedSize () - 
                           UdpHeader ().GetSerializedSize () - 
                           ESLRRoutingHeader ().GetSerializedSize ()
                          ) / ESLRrum ().GetSerializedSize ();

        Ptr<Packet> p = Create<Packet> ();
        SocketIpTtlTag tag;
        p->RemovePacketTag (tag);
        tag.SetTtl (0);
        p->AddPacketTag (tag);


        hdr.SetCommand (eslr::RU);
        hdr.SetRuCommand (eslr::REQUEST);
        hdr.SetRoutingTableRequestType (eslr::NE);
        hdr.SetAuthType (it->first->GetAuthType ()); // The Authentication type registered to the Nbr
        hdr.SetAuthData (it->first->GetAuthData ()); // The Authentication phrase registered to the Nbr

        RoutingTable::RoutesI rtIter;
        for (rtIter = routes.begin (); rtIter != routes.end (); rtIter++)
        {
					std::cout << "there are routes " << rtIter->first->GetDestNetwork () << std::endl;

          // No split Horizon is considered in this          	
				  rum.SetSequenceNo (1); // Since this is a requesting message, no deed to consider the sequence number 
          rum.SetMatric (0); // As disconnected the delay is 0
          rum.SetDestAddress (rtIter->first->GetDestNetwork ());
          rum.SetDestMask (rtIter->first->GetDestNetworkMask ());

          hdr.AddRum (rum);
          if (hdr.GetNoe () == maxRum)
          {
            p->AddHeader (hdr);
            NS_LOG_LOGIC ("ESLR: SendTo: " << *p);
            
            // send it via link local broadcast
            Ipv4Address broadAddress = it->first->GetNeighborAddress ().GetSubnetDirectedBroadcast (it->first->GetNeighborMask ());
            it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_MULT_PORT));
            p->RemoveHeader (hdr);
            hdr.ClearRums ();
          }
        }
        if (hdr.GetNoe () > 0)
        {
          p->AddHeader (hdr);
          NS_LOG_LOGIC ("ESLR: SendTo: " << *p);
          
          // send it via link local broadcast  
          Ipv4Address broadAddress = it->first->GetNeighborAddress ().GetSubnetDirectedBroadcast (it->first->GetNeighborMask ());   
          it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_MULT_PORT));
        }
      }
    }    
    // Finally clear out the created table instance of the main routing table.
    routes.clear ();
    // Clear the temporary neighbor table instance
    tempNeighbor.clear ();
	}
}

void
EslrRoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << " interface " << interface << " address " << address);

  if (!m_ipv4->IsUp (interface))
  {
    return;
  }

  if (m_interfaceExclusions.find (interface) != m_interfaceExclusions.end ())
  {
    return;
  }

	Ipv4Mask netMask = address.GetMask();
  Ipv4Address networkAddress = address.GetLocal().CombineMask(netMask);

  if (address.GetLocal() != Ipv4Address () && netMask != Ipv4Mask ())
  {
    AddNetworkRouteTo (networkAddress, 
                        netMask, 
                        interface, 
                        0, 0, 
                        eslr::PRIMARY, 
                        eslr::MAIN, 
                        Seconds (0), Seconds (0), Seconds (0));
  }

  SendTriggeredRouteUpdate ();
}

void 
EslrRoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << " interface " << interface << " address " << address);

  if (!m_ipv4->IsUp (interface))
  {
    return;
  }

  if (address.GetScope() != Ipv4InterfaceAddress::GLOBAL)
  {
    return;
  }
  
  // NOTE:
  // This is not necessary though
  InvalidateRoutesForInterface (interface, eslr::BACKUP);
  InvalidateRoutesForInterface (interface, eslr::MAIN);

  if (m_interfaceExclusions.find (interface) == m_interfaceExclusions.end ())
  {
    SendTriggeredRouteUpdate ();
  }
}

void 
EslrRoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION (this << ipv4);

  NS_ASSERT (m_ipv4 == 0 && ipv4 != 0);

  uint32_t i = 0;
  m_ipv4 = ipv4;
  m_nodeId  = m_ipv4->GetObject<Node> ()->GetId ();

  for (i = 0; i < m_ipv4->GetNInterfaces (); i++)
  {
    if (m_ipv4->IsUp (i))
    {
      NotifyInterfaceUp (i);
    }
    else
    {
      NotifyInterfaceDown (i);
    }
  }
}

void 
EslrRoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const
{
  NS_LOG_FUNCTION (this << stream);

  std::ostream* os = stream->GetStream ();

	if (m_print == eslr::N_TABLE)
	{
  	*os << "Node: " << GetObject<Node> ()->GetId ()
    	  << " Time: " << Simulator::Now ().GetSeconds () << "s "
     		<< "ESLR Neighbor Table" << '\n';
  	m_neighborTable.PrintNeighborTable (stream);
	}
	else if (m_print == eslr::MAIN_R_TABLE)
	{
		*os << "Node: " << GetObject<Node> ()->GetId ()
    	  << " Time: " << Simulator::Now ().GetSeconds () << "s "
     		<< "ESLR Main Routing Table" << '\n';
		m_routing.PrintRoutingTable (stream, eslr::MAIN);
	}
	else if (m_print == eslr::BACKUP_R_TABLE)
	{
		*os << "Node: " << GetObject<Node> ()->GetId ()
    	  << " Time: " << Simulator::Now ().GetSeconds () << "s "
     		<< "ESLR Backup Routing Table" << '\n';
		m_routing.PrintRoutingTable (stream, eslr::BACKUP);
	}
}

void
EslrRoutingProtocol::SendHelloMessageForInterface (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);

  Ptr<Packet> p = Create<Packet> ();
  SocketIpTtlTag tag;
  tag.SetTtl (1);
  p->AddPacketTag (tag);

  ESLRRoutingHeader hdr;
  hdr.SetCommand (eslr::KAM);
  hdr.SetRuCommand (eslr::NO);
  hdr.SetRoutingTableRequestType (eslr::NON);

  KAMHeader helloHdr;
  helloHdr.SetCommand (eslr::HELLO);
  helloHdr.SetAuthType (eslr::PLAIN_TEXT); // Administrator needs to decide the authentication type
  helloHdr.SetAuthData (1234); // Administrator needs to decide the authentication phrase
  helloHdr.SetNeighborID ((uint16_t)genarateNeighborID ());
  helloHdr.SetIdentifier ((uint8_t) genarateNeighborID ()); //FIXME for a random identifier
  
  // Get the interface attributes
  Ipv4InterfaceAddress iface = m_ipv4->GetAddress (interface, 0);
  Ipv4Address interfaceAddress= iface.GetLocal ();
  Ipv4Mask interfaceNetMask = iface.GetMask ();

  helloHdr.SetGateway (interfaceAddress);
  helloHdr.SetGatewayMask (interfaceNetMask);

  // Set & send the hello message
  hdr.AddKam (helloHdr);
  p->AddHeader (hdr);

  NS_LOG_LOGIC ("ESLR: SendTo: " << iface.GetBroadcast () << " " << *p);
	GetSocketForInterface (interface)->SendTo (p, 0, InetSocketAddress (iface.GetBroadcast (), ESLR_BROAD_PORT)); 
}

void 
EslrRoutingProtocol::SendHelloMessage ()
{
	NS_LOG_FUNCTION (this);
	
  Ptr<Packet> p = Create<Packet> ();
  SocketIpTtlTag tag;
  tag.SetTtl (1);
  p->AddPacketTag (tag);

  ESLRRoutingHeader hdr;
  hdr.SetCommand (eslr::KAM);
  hdr.SetRuCommand (eslr::NO);
  hdr.SetRoutingTableRequestType (eslr::NON);

  // Note:
  // At this moment, as the neighbors are not yet discovered,
  // we used the created socket list to broadcast hello messages
  for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
  {
    KAMHeader helloHdr;
    uint32_t interface = iter->second;

    helloHdr.SetCommand (eslr::HELLO); 
    helloHdr.SetAuthType (eslr::PLAIN_TEXT); // Administrator needs to decide the authentication type
    helloHdr.SetAuthData (1234); // Administrator needs to decide the authentication phrase
    helloHdr.SetNeighborID ((uint16_t)genarateNeighborID ());
    helloHdr.SetIdentifier ((uint8_t) genarateNeighborID ()); //FIXME for a random identifier
 
    // Get the interface attributes
    Ipv4InterfaceAddress iface = m_ipv4->GetAddress (interface, 0);
    Ipv4Address interfaceAddress= iface.GetLocal ();
    Ipv4Mask interfaceNetMask = iface.GetMask ();

    helloHdr.SetGateway (interfaceAddress);
    helloHdr.SetGatewayMask (interfaceNetMask);

    // Set & send the hello message
    hdr.AddKam (helloHdr);
    p->AddHeader (hdr);
 
    if (m_interfaceExclusions.find (interface) == m_interfaceExclusions.end ())
    {
      NS_LOG_LOGIC ("ESLR: SendTo: " << iface.GetBroadcast () << " " << *p);
 
      iter->first->SendTo (p, 0, InetSocketAddress (iface.GetBroadcast (), ESLR_BROAD_PORT));
      p->RemoveHeader (hdr);
      hdr.ClearKams ();
    }		
	}
}

void EslrRoutingProtocol::sendKams ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> p = Create<Packet> ();
  SocketIpTtlTag tag;

  //p->RemovePacketTag (tag);
  tag.SetTtl (1);
  p->AddPacketTag (tag);
  
  // NoTE:
  //      In this method, we assumed that number of interfaces of a router is not exceeding Maximum RUMs that a 
  //      ESLRRouting header supports. Therefore, we do not consider of calculating the number of 
  //      RUMs that include in to an ESLRouting header. 
  //      However, following equation can be used to calculate maximum number of RUMs 
  //      that can be added to a ESLR routing header
  //
  // uint16_t mtu = m_ipv4->GetMtu (interface);
  // uint16_t maxRum = (mtu - Ipv4Header ().GetSerializedSize () - UdpHeader ().GetSerializedSize () - ESLRRoutingHeader ().GetSerializedSize ()) / ESLRrum ().GetSerializedSize ();

  ESLRRoutingHeader hdr;
  hdr.SetCommand (eslr::KAM);
  hdr.SetRuCommand (eslr::NO);
  hdr.SetRoutingTableRequestType (eslr::NON);

  // acquiring an instance of the neighbor table
  NeighborTable::NeighborTableInstance tempNeighbor;
  m_neighborTable.ReturnNeighborTable (tempNeighbor);
  NeighborTable::NeighborI it;

  KAMHeader kamHdr;

  for (it = tempNeighbor.begin ();  it != tempNeighbor.end (); it++)
  {
 		hdr.SetAuthType (it->first->GetAuthType ());
  	hdr.SetAuthData (it->first->GetAuthData ());

    kamHdr.SetCommand (eslr::HI);
    kamHdr.SetAuthType (it->first->GetAuthType ());
    kamHdr.SetAuthData (it->first->GetAuthData ());
    kamHdr.SetNeighborID ((uint16_t)genarateNeighborID ());
    kamHdr.SetIdentifier ((uint8_t) genarateNeighborID ());

    // Get the interface attributes
    Ipv4InterfaceAddress iface = m_ipv4->GetAddress (it->first->GetInterface (), 0);
    Ipv4Address interfaceAddress= iface.GetLocal ();
    Ipv4Mask interfaceNetMask = iface.GetMask ();

    kamHdr.SetGateway (interfaceAddress);
    kamHdr.SetGatewayMask (interfaceNetMask);


    // Set & send the packet
    hdr.AddKam (kamHdr);
    p->AddHeader (hdr);

    // send it via link local broadcast
    Ipv4Address broadAddress = iface.GetBroadcast ();
    it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_BROAD_PORT));

      p->RemoveHeader (hdr);
      hdr.ClearKams ();		
	}

	//Clear the tempory neighbor table instanse
	tempNeighbor.clear ();
	
  // Reschedule next KAM
	Time sendKam = Seconds (m_rng->GetValue (0, m_kamTimer.GetSeconds ()));
  m_nextKeepAliveMessage = Simulator::Schedule (sendKam, &EslrRoutingProtocol::sendKams, this);
}

void 
EslrRoutingProtocol::SendTriggeredRouteUpdate ()
{
  NS_LOG_FUNCTION (this);

  if (m_nextTriggeredUpdate.IsRunning())
  {
    NS_LOG_LOGIC ("ESLR: Skipping Triggered Update due to cool-down");
    return;
  }

  // Note:  This part is directly inherited from the RFC 2080
  //        After a triggered update is sent, a timer is set between 1s to 5s. 
  //        between that time, any other triggered updates are not sent. 
  //        In addition, triggered updates are omitted if there is a scheduled periodic update.
  //        Furthermore, only changed routes will be sent.
  //        routes are marked when those get invalidated and disconnected. However, as a persistent protocol,
  //        ESLR only advertise disconnected routes.

  Time delay = Seconds (m_rng->GetValue (m_minTriggeredCooldownDelay.GetSeconds (),
                        m_maxTriggeredCooldownDelay.GetSeconds ()));
                        
  m_nextTriggeredUpdate = Simulator::Schedule (delay, &EslrRoutingProtocol::DoSendRouteUpdate, 
                                               this, eslr::TRIGGERED);
}

void 
EslrRoutingProtocol::SendPeriodicUpdate (void)
{
  NS_LOG_FUNCTION (this);    

  if (m_nextTriggeredUpdate.IsRunning ())
    m_nextTriggeredUpdate.Cancel ();

  DoSendRouteUpdate (eslr::PERIODIC);

  Time delay = m_periodicUpdateDelay + Seconds (m_rng->GetValue (0, m_periodicUpdateDelay.GetSeconds ()));
  m_nextPeriodicUpdate = Simulator::Schedule (delay, &EslrRoutingProtocol::SendPeriodicUpdate, this);
}

void 
EslrRoutingProtocol::DoSendRouteUpdate (eslr::UpdateType updateType)
{
  NS_LOG_FUNCTION (this);

  ESLRRoutingHeader hdr;

  if (updateType == eslr::PERIODIC)
  {
    NS_LOG_LOGIC ("ESLR: Periodic Update");
		hdr.SetPeriodicUpdate (true);    
  }
  else if (updateType == eslr::TRIGGERED)  
  {
    NS_LOG_LOGIC ("ESLR: Triggered Update");
  		hdr.SetTrigUpdate (true);
  }

  // acquiring an instance of the neighbor table
  NeighborTable::NeighborTableInstance tempNeighbor;
  m_neighborTable.ReturnNeighborTable (tempNeighbor);
  NeighborTable::NeighborI it;

  // acquiring an instance of the main routing table
  // NOTE:
  //  This instance is a separately created fresh instance of the M-table. 
  //  remove it after you use it.
  //  Done to accelerate the accessibility of the main routing table.
	//  However, in ns-3, this method does not do any effect to the performance, 
	//  hence it increases the memory usage.
	//  In order to use this method more effective, we have to use threading.
  RoutingTable::RoutingTableInstance tempMainTable;
  m_routing.ReturnRoutingTable (tempMainTable, eslr::MAIN);
  RoutingTable::RoutesI rtIter;

  for (it = tempNeighbor.begin ();  it != tempNeighbor.end (); it++)
  {
    uint32_t interface = it->first->GetInterface ();

    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
    Ipv4InterfaceAddress iface = l3->GetAddress (interface,0);

    if (m_interfaceExclusions.find (interface) == m_interfaceExclusions.end ())
    {
      // Calculating the Number of RUMs that can add to the ESLR Routing Header
      uint16_t mtu = m_ipv4->GetMtu (interface);
      uint16_t maxRum = (mtu - 
                         Ipv4Header ().GetSerializedSize () - 
                         UdpHeader ().GetSerializedSize () - 
                         ESLRRoutingHeader ().GetSerializedSize ()
                        ) / ESLRrum ().GetSerializedSize ();

      Ptr<Packet> p = Create<Packet> ();
      SocketIpTtlTag tag;
      p->RemovePacketTag (tag);
      tag.SetTtl (0);
      p->AddPacketTag (tag);

      hdr.SetCommand (eslr::RU);
      hdr.SetRuCommand (eslr::RESPONSE);
      hdr.SetRoutingTableRequestType (eslr::NON);
      hdr.SetAuthType (it->first->GetAuthType ()); // The Authentication type registered to the Nbr
      hdr.SetAuthData (it->first->GetAuthData ()); // The Authentication phrase registered to the Nbr

      for (rtIter = tempMainTable.begin (); rtIter != tempMainTable.end (); rtIter++)
      {
        bool splitHorizoning = (rtIter->first->GetInterface () == interface);

        bool isLocalHost = ((rtIter->first->GetDestNetwork () == "127.0.0.1") && 
                            (rtIter->first->GetDestNetworkMask () == Ipv4Mask::GetOnes ()));

        // NOTE:  
        //    All split-horizon routes are omitted.
        //    Route about the local host is omitted.
        //    Only changed routes are considered to reduce the advertisement packet size.
        if ((m_splitHorizonStrategy != (SPLIT_HORIZON && splitHorizoning)) && 
            (!isLocalHost) &&
            (updateType == eslr::PERIODIC || rtIter->first->GetRouteChanged ()))
        { 
          ESLRrum rum;
          if (rtIter->first->GetValidity () == eslr::INVALID)
            continue; // ignore the invalid route. 
          else if (rtIter->first->GetValidity () == eslr::VALID)
          {
            hdr.SetCbit (true);
						rum.SetCbit (true);
          }
          else if (rtIter->first->GetValidity () == eslr::DISCONNECTED)
          {
            hdr.SetDbit (true);
						rum.SetDbit (true);
          } 
					rum.SetSequenceNo (rtIter->first->GetSequenceNo () + 1);           
          rum.SetMatric (rtIter->first->GetMetric ());
          rum.SetDestAddress (rtIter->first->GetDestNetwork ());
          rum.SetDestMask (rtIter->first->GetDestNetworkMask ());

          hdr.AddRum (rum);
        }
        if (hdr.GetNoe () == maxRum)
        {
          p->AddHeader (hdr);
          NS_LOG_LOGIC ("SendTo: " << *p);
          
          // send it via link local broadcast
          it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (iface.GetBroadcast (), ESLR_MULT_PORT));
          p->RemoveHeader (hdr);
          hdr.ClearRums ();
        }
      }
      if (hdr.GetNoe () > 0)
      {
        p->AddHeader (hdr);
        NS_LOG_LOGIC ("SendTo: " << *p);
        // send it via link local broadcast     
        it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (iface.GetBroadcast (), ESLR_MULT_PORT));
      }
    }
  }
  
  // After each update, change the changed routes in to unchanged status.
  m_routing.ToggleRouteChanged ();
  
  // In order to synchronize the SeqNo of local routes, increment those.
  m_routing.IncrementSeqNo ();
  
  // Finally clear out the created table instance of the main routing table.
  tempMainTable.clear ();
  // Clear the temporary neighbor table instance
  tempNeighbor.clear ();
}

void 
EslrRoutingProtocol::Receive (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet = socket->Recv ();

  NS_LOG_LOGIC ("ESLR: Received " << *packet);

  Ipv4PacketInfoTag interfaceInfo;
  if (!packet->RemovePacketTag (interfaceInfo))
  {
    NS_ABORT_MSG ("ESLR: No TTL tag information attached for ESLR message, aborting.");
  }

  SocketIpTtlTag TtlInfoTag;
  if (!packet->RemovePacketTag (TtlInfoTag))
  {
    NS_ABORT_MSG ("ESLR: No incoming interface on ESLR message, aborting.");
  }

  SocketAddressTag tag;
  if (!packet->RemovePacketTag (tag))
  {
    NS_ABORT_MSG ("ESLR: No incoming sender address on ESLR message, aborting.");
  }

  uint32_t incomingIf = interfaceInfo.GetRecvIf ();
  Ptr<Node> node = this->GetObject<Node> ();
  Ptr<NetDevice> dev = node->GetDevice (incomingIf);

  int32_t ipInterfaceIndex = m_ipv4->GetInterfaceForDevice (dev);

  Ipv4Address senderAddress = InetSocketAddress::ConvertFrom (tag.GetAddress ()).GetIpv4 ();
  uint16_t senderPort = InetSocketAddress::ConvertFrom (tag.GetAddress ()).GetPort ();

  int32_t interfaceForAddress = m_ipv4->GetInterfaceForAddress (senderAddress);

  if (interfaceForAddress != -1)
  {
    NS_LOG_LOGIC ("ESLR: A piggybacked packet, Ignoring it.");
    return;
  }

  NS_LOG_LOGIC ("ESLR: Handle the request packet.");
  
  ESLRRoutingHeader hdr;
  packet->RemoveHeader (hdr);
  if (hdr.GetCommand () == eslr::KAM)
  {
    // No security is considered in this phase
    NS_LOG_LOGIC ("ESLR: Handle the KAM packet.");

    // Get the actual bounded socket for the received interface.
    // Multicast addresses are bound to an another public socket
    // However, Neighbors should represent the actual received interface index and 
    // the socket bounded to the interface.
    // neighbor table maintains the socket and interface combination
		ipInterfaceIndex = GetInterfaceForSocket (socket);

  	if (ipInterfaceIndex == -1)
  	{
  	  NS_LOG_LOGIC ("ESLR: No incoming interface on ESLR message, returning.");
			return;
  	} 

    HandleKamRequests (hdr, senderAddress, ipInterfaceIndex);
  }
  else if (hdr.GetCommand () == eslr::RU)
  {
    NeighborTable::NeighborI neighborRecord;
    bool isNeighborPresent = m_neighborTable.FindNeighborForAddress (senderAddress, neighborRecord);
    if (isNeighborPresent)
    {
			//FIXME : A simple fix, but not correct for every possibility
			// probably a bug
      if (neighborRecord->first->GetValidity () == eslr::INVALID && hdr.GetAdvertisementType () != 0)
      {
        NS_LOG_LOGIC ("ESLR:An invalid neighbor " << senderAddress);
        return;
      }
     
      m_protocolMessages++; // Increment the debug message counter.
      if (hdr.GetRuCommand () == eslr::REQUEST) // This could be either VOID state or VALID state
      {
        // Authentication is not checked for the route requests if the state is VOID
        if ((neighborRecord->first->GetValidity () == eslr::VOID) ||
            ((neighborRecord->first->GetValidity () == eslr::VALID) &&
             (hdr.GetAuthType () == neighborRecord->first->GetAuthType ()) &&
             (hdr.GetAuthData () == neighborRecord->first->GetAuthData ())))
        {
          HandleRouteRequests (hdr, senderAddress, senderPort, ipInterfaceIndex);
        }
        else
        {
          NS_LOG_LOGIC ("ESLR: Authentication FAILED for " << senderAddress);
          return;          
        }
      }
      else if (hdr.GetRuCommand () == eslr::RESPONSE)
      {
        if ((neighborRecord->first->GetValidity () == eslr::VALID) &&
             (hdr.GetAuthType () == neighborRecord->first->GetAuthType ()) &&
             (hdr.GetAuthData () == neighborRecord->first->GetAuthData ()))
        {
					if (hdr.GetFastTrigUpdate ())
					{
						std::cout << m_nodeId << " received a Fast triggered update message from " << senderAddress << std::endl;					
						HandleFastTrigUpdates (hdr, senderAddress, ipInterfaceIndex);
					}
					else if (hdr.GetPeriodicUpdate () || hdr.GetTrigUpdate () || hdr.GetAdvertisementType () == 0)
					{
						// For periodic, triggered, and route response messages, ESLR shares the same method
          	HandleRouteResponses (hdr, senderAddress, ipInterfaceIndex);       
					}
					else
					{
						NS_LOG_LOGIC ("ESLR: Not a supporting advertisement message. Returning! ");
						return;
					}       
        }
        else
        {
          NS_LOG_LOGIC ("ESLR:Authentication FAILED for " << senderAddress);
          return;           
        }	
      }      
    }
    else
    {
      NS_ABORT_MSG (m_nodeId << " Sender " << senderAddress << " is not a neighbor of me, aborting!");
    }
  }
  else if (hdr.GetCommand () == eslr::SRC)
  {
    // TODO: Authentication is not yet implemented    
    HandleSrcRequests (hdr, senderAddress, ipInterfaceIndex);
  }
  else
  {
    NS_LOG_LOGIC ("ESLR: Ignoring message with unknown command: " << int (hdr.GetCommand ()));
  }
  return;
}

void 
EslrRoutingProtocol::HandleFastTrigUpdates (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface)
{
  NS_LOG_FUNCTION (this << senderAddress << incomingInterface << hdr);

  if (m_interfaceExclusions.find (incomingInterface) != m_interfaceExclusions.end ())
  { 
    NS_LOG_DEBUG ("ESLR: Ignoring an update message from an excluded interface: " << incomingInterface);
    return;
  }

  std::list<ESLRrum> rums = hdr.GetRumList ();
  bool invalidatedInMain = false, invalidatedInBakcup = false;

  // acquiring an instance of the neighbor table
  NeighborTable::NeighborTableInstance tempNeighbor;
  m_neighborTable.ReturnNeighborTable (tempNeighbor);
  NeighborTable::NeighborI itNbr;
  
  //Bypass the triggered update sequence and send a Fast triggered update message
  NS_LOG_DEBUG ("ESLR: Bypass the existing triggered hold-down");
  if (m_nextTriggeredUpdate.IsRunning ())
    m_nextTriggeredUpdate.Cancel (); 
    
  // Create Fast triggered update message
  Ptr<Packet> p = Create<Packet> ();
  SocketIpTtlTag tag;
  p->RemovePacketTag (tag);
  tag.SetTtl (0);
  p->AddPacketTag (tag);

  ESLRRoutingHeader hdrSend;
  hdrSend.SetCommand (eslr::RU);
  hdrSend.SetRuCommand (eslr::RESPONSE);
  hdrSend.SetRoutingTableRequestType (eslr::NON);
  hdrSend.SetFastTrigUpdate (true);
  hdrSend.SetDbit (true);
       
  ESLRrum rum;
  rum.SetSequenceNo (1); // reset the sequence number.
  rum.SetMatric (0); // Since no zero delay is available, zero is considered as disconnected.
  
  // Even though a fast triggered update contains only one update
  // this method use an iterator for looping, 
  // because in future versions we will add multiple messages about broken links  
  for (std::list<ESLRrum>::iterator itRum = rums.begin (); itRum != rums.end (); itRum++)
  {
    // Ignore updates about my interfaces
    if (m_routing.IsLocalRouteAvailable (itRum->GetDestAddress (), itRum->GetDestMask ()))
    {
      NS_LOG_LOGIC ("ESLR: Route is about my local network. Skip the RUM");
      continue;
    }
    //if (it->GetDbit ()) //if ((it->GetSequenceNo () % 2) != 0)
    //{
        NS_LOG_DEBUG ("ESLR: Invalidating all broken routes.");

			  RoutingTable::RoutesI foundMroute;
			  bool mRouteFound = false;

			  foundMroute = m_routing.FindValidRouteRecordForDestination (itRum->GetDestAddress (),
																																	  itRum->GetDestMask (),
																																	  senderAddress,
																																	  mRouteFound,
																																	  eslr::MAIN);
        invalidatedInBakcup = InvalidateBrokenRoute (itRum->GetDestAddress (),
                                                     itRum->GetDestMask (),
                                                     senderAddress, eslr::BACKUP);
        invalidatedInMain = InvalidateBrokenRoute (itRum->GetDestAddress (),
                                                   itRum->GetDestMask (),
                                                   senderAddress, eslr::MAIN);
			
			  if (mRouteFound && invalidatedInMain)
			  {
			    rum.SetDestAddress (itRum->GetDestAddress ());
          rum.SetDestMask (itRum->GetDestMask ());
          
          NS_LOG_LOGIC ("ESLR: SendTo: " << *p);
          
	        for (itNbr = tempNeighbor.begin ();  itNbr != tempNeighbor.end (); itNbr++)
          {
						// NOTE:
            // apply Split horizon based on 
            // 1. incoming interface 
            // 2. actual interface that the route is learned
						// Event Split horizon is not enables, this method consider it in order to reduce the burden
		        if (itNbr->first->GetInterface () != incomingInterface || itNbr->first->GetInterface () != foundMroute->first->GetInterface ())
		        {
	            if (m_interfaceExclusions.find (incomingInterface) == m_interfaceExclusions.end ())
	            {
				        // Authentication is necessary
          			hdrSend.SetAuthType (itNbr->first->GetAuthType ()); 
          			hdrSend.SetAuthData (itNbr->first->GetAuthData ());
								
								hdrSend.AddRum (rum);
          			p->AddHeader (hdrSend);

                // send it via link local broadcast
                NS_LOG_LOGIC ("ESLR: Send a fast triggered update to " << itNbr->first->GetNeighborAddress ());
                
                Ipv4Address broadAddress = itNbr->first->GetNeighborAddress ().GetSubnetDirectedBroadcast (itNbr->first->GetNeighborMask ());
                itNbr->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_MULT_PORT));

				        p->RemoveHeader (hdrSend);
								hdrSend.ClearRums ();
              }
            }
	        }
			  }
			  else
			  {
			    NS_LOG_LOGIC ("ESLR: " << m_nodeId << " Advertisement is already received and ignoring it");
			    std::cout << m_nodeId << " Advertisement: " << itRum->GetDestAddress () << " is received from " << senderAddress <<" and ignoreing it " << std::endl;
			  }
		//}
	}

  //Clear the temporary neighbor table instance
  tempNeighbor.clear ();

	// Re schedule the triggered update
	NS_LOG_DEBUG ("ESLR: Reset the triggered hold-down");
	Time delay = Seconds (m_rng->GetValue (	m_minTriggeredCooldownDelay.GetSeconds (),
	                      									m_maxTriggeredCooldownDelay.GetSeconds ()));
	                      									
  m_nextTriggeredUpdate = Simulator::Schedule ( delay, &EslrRoutingProtocol::DoSendRouteUpdate,
                                                this, eslr::TRIGGERED);
}

void 
EslrRoutingProtocol::HandleSrcRequests (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface)
{
  NS_LOG_FUNCTION (this << senderAddress << incomingInterface << hdr);

  // NOTE: 
  //  Interface exclusion is not considered in this section.
  //  As the advertisement is directly coming from the server, even the interface is excluded,
  //  We have to accept the advertisement.
   
  std::list<SRCHeader> srcs = hdr.GetSrcList ();  

  uint32_t SCost = 0;
  double tempCost = 0.0;
  
  if (srcs.empty ())
  {
    NS_LOG_LOGIC ("ESLR: No Server Advertisement Messages attached.");
    return;    
  }
  
  for (std::list<SRCHeader>::iterator it = srcs.begin (); it != srcs.end (); it++)
  {
    if (m_routing.IsLocalRouteAvailable (it->GetserverAddress ().CombineMask (it->GetNetMask ()), it->GetNetMask ()))
    {
      NS_LOG_LOGIC ("ESLR: calculate server cost");
      
      tempCost = 1.0 / (it->GetMue () - it->GetLambda ());
      SCost = uint32_t (m_K1 * tempCost * 1000000); // convert the value to microseconds and scale up it using K1
          
      m_routing.UpdateLocalRoute (it->GetserverAddress ().CombineMask (it->GetNetMask ()), 
                                  it->GetNetMask (),
                                  SCost);
    }
    else
    {
      NS_LOG_LOGIC ("ESLR: " << it->GetserverAddress ().CombineMask (it->GetNetMask ()) 
                             << "Route is not about my local network. Skip the SRC");
    }
  }
}

void 
EslrRoutingProtocol::HandleKamRequests (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface)
{
  NS_LOG_FUNCTION (this << senderAddress <<  incomingInterface  << hdr);
	std::list<KAMHeader> kams = hdr.GetKamList ();

  if (kams.empty ())
  {
    NS_LOG_LOGIC ("ESLR: No Keep Alive Messages are attached.");
    return;
  }

  if (m_interfaceExclusions.find (incomingInterface) == m_interfaceExclusions.end ())
  {
    // This method is implemented by assuming the future version of the ESLR:KAMs 
    // which may contain multiple messages.
    for (std::list<KAMHeader>::iterator iter = kams.begin (); iter != kams.end (); iter++)
    {
			if (iter->GetGateway () == "0.0.0.0")
			{
				//FIXME 
				// This is a bug. 
				//TODO
				// Want to track it down
				continue;
			}
			// For Hello messages
		  bool neighborPresent = m_neighborTable.FindVoidNeighbor (iter->GetNeighborID ());
			if (iter->Getcommand () == HELLO && neighborPresent)
			{
				NS_LOG_DEBUG ("ESLR: Neighbor is present " << iter->GetNeighborID () << " Ignore the update! ");
				continue;
			}
			else if (iter->Getcommand () == HELLO && !neighborPresent)
			{
				NS_LOG_DEBUG ("ESLR: Add a new record to Neighbor and set a Timer to delete" <<
												iter->GetNeighborID () <<
												iter->GetGateway ());
        
				Ptr<Socket> receivedSocket = GetSocketForInterface (incomingInterface);
        
				NeighborTableEntry* newNeighbor = new  NeighborTableEntry ( iter->GetNeighborID (), iter->GetGateway (),
                                                                    iter->GetGatewayMask (), incomingInterface,
                                                                    receivedSocket, iter->GetAuthType (),
                                                                    iter->GetAuthData (), iter->GetIdentifier (),
																																		eslr::VOID);
				m_neighborTable.AddVoidNeighbor (newNeighbor, m_neighborTimeoutDelay);

				NS_LOG_DEBUG ("ESLR: Send a Hello message to newly discovered neighbor");
  			
				// Hello procedure
				Ptr<Packet> p = Create<Packet> ();
  			SocketIpTtlTag tag;
  			tag.SetTtl (1);
  			p->AddPacketTag (tag);

  			ESLRRoutingHeader hdr;
  			hdr.SetCommand (eslr::KAM);
  			hdr.SetRuCommand (eslr::NO);
  			hdr.SetRoutingTableRequestType (eslr::NON);
  			hdr.SetAuthType (iter->GetAuthType ());
  			hdr.SetAuthData (iter->GetAuthData ());

				KAMHeader helloHdr;
				helloHdr.SetCommand (eslr::HELLO);
    		helloHdr.SetAuthType (iter->GetAuthType ());
    		helloHdr.SetAuthData (iter->GetAuthData ());
		    helloHdr.SetNeighborID ((uint16_t)genarateNeighborID ());
    		helloHdr.SetIdentifier (iter->GetIdentifier ());

    		Ipv4InterfaceAddress iface = m_ipv4->GetAddress (incomingInterface, 0);
    		Ipv4Address interfaceAddress= iface.GetLocal ();
    		Ipv4Mask interfaceNetMask = iface.GetMask ();

    		helloHdr.SetGateway (interfaceAddress);
    		helloHdr.SetGatewayMask (interfaceNetMask);

				hdr.AddKam (helloHdr);
				p->AddHeader (hdr);
				if (m_interfaceExclusions.find (incomingInterface) == m_interfaceExclusions.end ())
				{
					NS_LOG_LOGIC ("ESLR: Send a hello message to: " << iface.GetBroadcast () << " " << *p);
					receivedSocket->SendTo (p, 0, InetSocketAddress (iface.GetBroadcast (), ESLR_BROAD_PORT));
				}
  
				// REQ procedure
				NS_LOG_DEBUG ("ESLR:  Create REQ message");

        Ptr<Packet> req = Create<Packet> ();
        req->AddPacketTag (tag);

        ESLRRoutingHeader reqHdr;
        reqHdr.SetCommand (eslr::RU);
        reqHdr.SetRuCommand (eslr::REQUEST);
        reqHdr.SetRoutingTableRequestType (eslr::ND);
        reqHdr.SetAuthType (iter->GetAuthType ());
        reqHdr.SetAuthData (iter->GetAuthData ());

    		// create a RUM and add some dummy data
    		ESLRrum rum;
 
    		rum.SetSequenceNo (0);
    		rum.SetMatric (0);
    		rum.SetDestAddress (Ipv4Address ());
    		rum.SetDestMask (Ipv4Mask ());

    		reqHdr.AddRum (rum);
    		req->AddHeader (reqHdr);	
				
				if (m_interfaceExclusions.find (incomingInterface) == m_interfaceExclusions.end ())
				{
					NS_LOG_LOGIC ("ESLR: Send a REQ message to: " << iface.GetBroadcast () << " " << *req);
					receivedSocket->SendTo (req, 
																	0, 
																	InetSocketAddress (
																	iter->GetGateway ().GetSubnetDirectedBroadcast (
																	iter->GetGatewayMask ()), 
																	ESLR_MULT_PORT));
				}
			}
			else if (iter->Getcommand () == HI)// ESLR KAM messages
			{
      	Ptr<Socket> receivedSocket = GetSocketForInterface (incomingInterface);
      	if (receivedSocket == 0)
      	{
        	NS_ABORT_MSG ("ESLR: No matching socket found for the incoming interface, aborting.");
      	}
        
				NeighborTable::NeighborI neighborRecord;
      	bool isNeighborPresent = m_neighborTable.FindNeighbor (iter->GetNeighborID (), neighborRecord);
				
				if (isNeighborPresent)
				{
        	NS_LOG_DEBUG ("ESLR: Updating the Neigbor" <<  
         	               neighborRecord->first->GetNeighborID () << 
         	               neighborRecord->first->GetNeighborAddress ());
	
					NeighborTableEntry* existingNeighbor = new NeighborTableEntry (	
    	                                               neighborRecord->first->GetNeighborID (),
  																									 neighborRecord->first->GetNeighborAddress (), 
																										 neighborRecord->first->GetNeighborMask (),
																										 neighborRecord->first->GetInterface (), 
																										 neighborRecord->first->GetSocket(), 
																										 neighborRecord->first->GetAuthType (), 
																										 neighborRecord->first->GetAuthData (),
																										 neighborRecord->first->GetIdentifier (),
					 																					 eslr::VALID);
					 																				 
					m_neighborTable.UpdateNeighbor (existingNeighbor, m_neighborTimeoutDelay, 
          	                              m_garbageCollectionDelay);	
      	}
				else
				{
					NS_LOG_DEBUG ("ESLR: A neighbor is note present for " << 
												 iter->GetNeighborID () << 
												 " returning !!ter->GetNeighborID ()");
					return ;
				}
			}
    }
  }
}

void 
EslrRoutingProtocol::HandleRouteRequests (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint16_t senderPort, uint32_t incomingInterface)
{
  NS_LOG_FUNCTION (this << senderAddress << senderPort << incomingInterface);
 
  eslr::EslrHeaderRequestType reqType = hdr.GetRoutingTableRequestType ();
  
  std::list<ESLRrum> rums = hdr.GetRumList ();
  
  if (rums.empty ())
  {
    NS_LOG_LOGIC ("ESLR: Ignoring an update message with no requests: " << incomingInterface);
    return;
  }

  if (m_interfaceExclusions.find (incomingInterface) != m_interfaceExclusions.end ())
  {
    NS_LOG_LOGIC ("ESLR: Ignoring an update message from an excluded interface: " << senderAddress);
    return;
  }  
 
	if (reqType == eslr::ND)
	{
		NS_LOG_DEBUG ("ESLR: REQ is received with ND enabled for " << senderAddress);

		NeighborTable::NeighborI neighborRecord;
    bool foundVoidNeighbor =  m_neighborTable.FindVoidNeighborForAddress (senderAddress, neighborRecord);

		if (!foundVoidNeighbor)
		{
    	NS_LOG_LOGIC ("ESLR: No void neighbor is found for : " << senderAddress);
    	return;	
		}	
		else
		{
			NS_LOG_DEBUG ("ESLR: Updating the Neigbor" <<
                     neighborRecord->first->GetNeighborID () <<
                     neighborRecord->first->GetNeighborAddress ());

      NeighborTableEntry* existingNeighbor = new NeighborTableEntry (
                                                   neighborRecord->first->GetNeighborID (),
                                                   neighborRecord->first->GetNeighborAddress (),
                                                   neighborRecord->first->GetNeighborMask (),
                                                   neighborRecord->first->GetInterface (),
                                                   neighborRecord->first->GetSocket(),
                                                   neighborRecord->first->GetAuthType (),
                                                   neighborRecord->first->GetAuthData (),
                                                   eslr::VALID);

      m_neighborTable.UpdateNeighbor (existingNeighbor, m_neighborTimeoutDelay,
                                          m_garbageCollectionDelay);		

			// Send the Entire MTable to the newly discovered neighbor 
			NS_LOG_DEBUG ("ESLR: Send routing table to " << senderAddress);
    	
			// acquiring an instance of the main routing table
    	RoutingTable::RoutingTableInstance tempMainTable;
    	m_routing.ReturnRoutingTable (tempMainTable, eslr::MAIN);
    	RoutingTable::RoutesI rtIter;
    	
			// Calculating the Number of RUMs that can add to the ESLR Routing Header
    	uint16_t mtu = m_ipv4->GetMtu (incomingInterface);
    	uint16_t maxRum = (mtu - 
    	                   Ipv4Header ().GetSerializedSize () - 
     	                   UdpHeader ().GetSerializedSize () - 
                       	 ESLRRoutingHeader ().GetSerializedSize ()) / ESLRrum ().GetSerializedSize ();

    	Ptr<Packet> p = Create<Packet> ();
    	SocketIpTtlTag tag;
    	p->RemovePacketTag (tag);
    	tag.SetTtl (0);
    	p->AddPacketTag (tag);

    	ESLRRoutingHeader hdr;
    	hdr.SetCommand (eslr::RU);
    	hdr.SetRuCommand (eslr::RESPONSE);
    	hdr.SetRoutingTableRequestType (eslr::NON);
    	hdr.SetAuthType (neighborRecord->first->GetAuthType ()); // The Authentication type registered to the Nbr
    	hdr.SetAuthData (neighborRecord->first->GetAuthData ()); // The Authentication phrase registered to the Nbr
    	hdr.SetAdvertisementTypeZero ();

    	for (rtIter = tempMainTable.begin (); rtIter != tempMainTable.end (); rtIter++)
    	{
     		bool splitHorizoning = (rtIter->first->GetInterface () == incomingInterface);

      	bool isLocalHost = ((rtIter->first->GetDestNetwork () == "127.0.0.1") && 
        	                  (rtIter->first->GetDestNetworkMask () == Ipv4Mask::GetOnes ()));

      	// Note: Split horizon is considered while responding to a route request
      	// In addition, only valid routes are considered for generating route update messages. 
      	if ((m_splitHorizonStrategy != (SPLIT_HORIZON && splitHorizoning)) && 
       	   (!isLocalHost) &&
       	   (rtIter->first->GetValidity () == eslr::VALID))
      	{
        	ESLRrum rum;
        	rum.SetSequenceNo (rtIter->first->GetSequenceNo () + 1);
        	rum.SetMatric (rtIter->first->GetMetric ());
        	rum.SetDestAddress (rtIter->first->GetDestNetwork ());
        	rum.SetDestMask (rtIter->first->GetDestNetworkMask ());
					rum.SetCbit (true);

        	hdr.AddRum (rum);
      	}
      	if (hdr.GetNoe () == maxRum)
      	{
        	NS_LOG_DEBUG ("ESLR: reply to the request came from " << senderAddress);      	
        	p->AddHeader (hdr);
       		 
        	Ipv4Address broadAddress = senderAddress.GetSubnetDirectedBroadcast (neighborRecord->first->GetNeighborMask ()); 
        	neighborRecord->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_BROAD_PORT));
        	
        	p->RemoveHeader (hdr);
        	hdr.ClearRums ();
      	}
    	}
    	if (hdr.GetNoe () > 0)
    	{
      	NS_LOG_DEBUG ("ESLR: reply to the request came from " << senderAddress);    	
      	p->AddHeader (hdr);
   
      	Ipv4Address broadAddress = senderAddress.GetSubnetDirectedBroadcast (neighborRecord->first->GetNeighborMask ());  
      	neighborRecord->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_BROAD_PORT));
    	}        
  
  		// Finally clear out the created table instance of the main routing table.
  		tempMainTable.clear ();  

  		// As neighbor Discovery is finish now, schedule KAM 
  		Time sendKam = Seconds (m_rng->GetValue (0, m_kamTimer.GetSeconds ()));
  		m_nextKeepAliveMessage = Simulator::Schedule (sendKam, 
																										&EslrRoutingProtocol::sendKams, 
																										this);	
		}
	}
	else
	{	
  	//Get the relevant neighbor information for the provided destination address
  	NeighborTable::NeighborI it;
  	bool foundNeighbor =  m_neighborTable.FindValidNeighborForAddress (senderAddress, it);
  
  	// acquiring an instance of the main routing table
  	RoutingTable::RoutingTableInstance tempMainTable;
  	m_routing.ReturnRoutingTable (tempMainTable, eslr::MAIN);
  	RoutingTable::RoutesI rtIter;  
  
  	if (!foundNeighbor)
  	{
    	NS_LOG_LOGIC ("ESLR: No neighbor found for the specified destination address, returning!.");
    	return;
  	}  

  	if (reqType == eslr::OE)
  	{
    	// As th request for a one route, no need of considering the split-horizon
    	NS_LOG_LOGIC("ESLR: " << senderAddress << " Requested only one record.");
    
   		Ptr<Packet> p = Create<Packet> ();
    	SocketIpTtlTag tag;
    	p->RemovePacketTag (tag);
    	tag.SetTtl (0);
    	p->AddPacketTag (tag);

    	ESLRRoutingHeader hdr;
    	hdr.SetCommand (eslr::RU);
    	hdr.SetRuCommand (eslr::RESPONSE);
    	hdr.SetRoutingTableRequestType (eslr::NON);
    	hdr.SetAuthType (it->first->GetAuthType ()); // The Authentication type registered to the Nbr
    	hdr.SetAuthData (it->first->GetAuthData ()); // The Authentication phrase registered to the Nbr
    
    	// find the route record matches the destination address given in the RUM
    	// in this case the valid routes are only considered.
    	// further, split-horizoning is not considered.  
    	RoutingTable::RoutesI foundRoute;
   	 	bool foundInMain = m_routing.FindValidRouteRecord (rums.begin ()->GetDestAddress (), 
                                                       rums.begin ()->GetDestMask (), 
                                                       foundRoute, eslr::MAIN);
    
    	if (!foundInMain)
    	{
      	NS_LOG_LOGIC ("ESLR: No route record found for the specified destination address, returning!.");
      	return;      
    	}
    
    	ESLRrum rum;

    	rum.SetSequenceNo (foundRoute->first->GetSequenceNo () + 1);
    	rum.SetMatric (foundRoute->first->GetMetric ());
   	 	rum.SetDestAddress (foundRoute->first->GetDestNetwork ());
    	rum.SetDestMask (foundRoute->first->GetDestNetworkMask ());

    	hdr.AddRum (rum);

    	p->AddHeader (hdr);
    	NS_LOG_DEBUG ("ESLR: reply to the request came from " << senderAddress);
 
    	Ipv4Address broadAddress = senderAddress.GetSubnetDirectedBroadcast (it->first->GetNeighborMask ());  
    	it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_MULT_PORT));    
  	}
  	else if (reqType == eslr::NE)
  	{
    	NS_LOG_LOGIC("ESLR: " << senderAddress << " Requested set of records.");
    
    	// Calculating the Number of RUMs that can add to the ESLR Routing Header
    	uint16_t mtu = m_ipv4->GetMtu (incomingInterface);
    	uint16_t maxRum = (mtu - 
                       Ipv4Header ().GetSerializedSize () - 
                       UdpHeader ().GetSerializedSize () -  
                       ESLRRoutingHeader ().GetSerializedSize ()
                      ) / ESLRrum ().GetSerializedSize ();    

    	Ptr<Packet> p = Create<Packet> ();
    	SocketIpTtlTag tag;
    	p->RemovePacketTag (tag);
    	tag.SetTtl (0);
    	p->AddPacketTag (tag);

    	ESLRRoutingHeader hdr;
    	hdr.SetCommand (eslr::RU);
    	hdr.SetRuCommand (eslr::RESPONSE);
    	hdr.SetRoutingTableRequestType (eslr::NON);
    	hdr.SetAuthType (it->first->GetAuthType ()); // The Authentication type registered to the Nbr
    	hdr.SetAuthData (it->first->GetAuthData ()); // The Authentication phrase registered to the Nbr

    	RoutingTable::RoutesI foundRoute;
    	bool foundInMain;        
    	for (std::list<ESLRrum>::iterator iter = rums.begin (); iter != rums.end (); iter++)
    	{
      	// check for the route's availability based on the destination address and mask.
      	foundInMain = m_routing.FindValidRouteRecord (iter->GetDestAddress (), 
       	                                             iter->GetDestMask (), 
       		                                           foundRoute, eslr::MAIN);
      
      	if (!foundInMain)
      	{
        	// As there are no route for the the given destination, continue to the next RUM
        	NS_LOG_LOGIC("ESLR: No routes found for the " << iter->GetDestAddress () << " continue...");
        	continue;        
      	}
      
      	ESLRrum rum;

      	// Splithorizon is not considered. 
      	rum.SetSequenceNo (foundRoute->first->GetSequenceNo () + 1);
      	rum.SetMatric (foundRoute->first->GetMetric ());
      	rum.SetDestAddress (foundRoute->first->GetDestNetwork ());
      	rum.SetDestMask (foundRoute->first->GetDestNetworkMask ());

      	hdr.AddRum (rum);      

      	if (hdr.GetNoe () == maxRum)
      	{
        	p->AddHeader (hdr);
        
        	NS_LOG_DEBUG ("ESLR: reply to the request came from " << senderAddress);
        
        	// use the link local broadcast
        	Ipv4Address broadAddress = senderAddress.GetSubnetDirectedBroadcast (it->first->GetNeighborMask ());  
        	it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_MULT_PORT));
        
        	p->RemoveHeader (hdr);
        	hdr.ClearRums ();
      	}      
    	}
    	if (hdr.GetNoe () > 0)
    	{
      	p->AddHeader (hdr);
      	NS_LOG_DEBUG ("ESLR: reply to the request came from " << senderAddress);
      
      	// use the link local broadcast
      	Ipv4Address broadAddress = senderAddress.GetSubnetDirectedBroadcast (it->first->GetNeighborMask ());  
      	it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_MULT_PORT));
    	} 
  	}
  	else if (reqType == eslr::ET)
  	{
    	NS_LOG_LOGIC("ESLR: " << senderAddress << " Requested entire routing table.");

    	// Calculating the Number of RUMs that can add to the ESLR Routing Header
    	uint16_t mtu = m_ipv4->GetMtu (incomingInterface);
    	uint16_t maxRum = (mtu - 
    	                   Ipv4Header ().GetSerializedSize () - 
     	                   UdpHeader ().GetSerializedSize () - 
                         ESLRRoutingHeader ().GetSerializedSize ()
                         ) / ESLRrum ().GetSerializedSize ();

    	Ptr<Packet> p = Create<Packet> ();
    	SocketIpTtlTag tag;
    	p->RemovePacketTag (tag);
    	tag.SetTtl (0);
    	p->AddPacketTag (tag);

    	ESLRRoutingHeader hdr;
    	hdr.SetCommand (eslr::RU);
    	hdr.SetRuCommand (eslr::RESPONSE);
    	hdr.SetRoutingTableRequestType (eslr::NON);
    	hdr.SetAuthType (it->first->GetAuthType ()); // The Authentication type registered to the Nbr
    	hdr.SetAuthData (it->first->GetAuthData ()); // The Authentication phrase registered to the Nbr
    
    	for (rtIter = tempMainTable.begin (); rtIter != tempMainTable.end (); rtIter++)
    	{
     		bool splitHorizoning = (rtIter->first->GetInterface () == incomingInterface);

      	bool isLocalHost = ((rtIter->first->GetDestNetwork () == "127.0.0.1") && 
        	                  (rtIter->first->GetDestNetworkMask () == Ipv4Mask::GetOnes ()));

      	// Note: Split horizon is considered while responding to a route request
      	// In addition, only valid routes are considered for generating route update messages. 
      	if ((m_splitHorizonStrategy != (SPLIT_HORIZON && splitHorizoning)) && 
       	   (!isLocalHost) &&
       	   (rtIter->first->GetValidity () == eslr::VALID))
      	{ 
        	ESLRrum rum;

        	rum.SetSequenceNo (rtIter->first->GetSequenceNo () + 1);
        	rum.SetMatric (rtIter->first->GetMetric ());
        	rum.SetDestAddress (rtIter->first->GetDestNetwork ());
        	rum.SetDestMask (rtIter->first->GetDestNetworkMask ());

        	hdr.AddRum (rum);
      	}
      	if (hdr.GetNoe () == maxRum)
      	{
        	p->AddHeader (hdr);
        
        	NS_LOG_DEBUG ("ESLR: reply to the request came from " << senderAddress);
       		 
        	Ipv4Address broadAddress = senderAddress.GetSubnetDirectedBroadcast (it->first->GetNeighborMask ());  
        	it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_MULT_PORT));
        	p->RemoveHeader (hdr);
        	hdr.ClearRums ();
      	}
    	}
    	if (hdr.GetNoe () > 0)
    	{
      	p->AddHeader (hdr);
      	NS_LOG_DEBUG ("ESLR: reply to the request came from " << senderAddress);
   
      	Ipv4Address broadAddress = senderAddress.GetSubnetDirectedBroadcast (it->first->GetNeighborMask ());  
      	it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (broadAddress, ESLR_MULT_PORT));
    	}        
  	  // In order to synchronize the SeqNo of local routes, increment those.
  		//m_routing.IncrementSeqNo ();
  
  		// Finally clear out the created table instance of the main routing table.
  		tempMainTable.clear ();  
		}
	}
}

void 
EslrRoutingProtocol::HandleRouteResponses (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface)
{
  NS_LOG_FUNCTION (this << senderAddress << incomingInterface << hdr);

  if (m_interfaceExclusions.find (incomingInterface) != m_interfaceExclusions.end ())
  {
    NS_LOG_DEBUG ("ESLR: Ignoring an update message from an excluded interface: " << incomingInterface);
    return;
  }
  
  std::list<ESLRrum> rums = hdr.GetRumList ();
	bool invalidatedInMain = false, invalidatedInBakcup = false;
  for (std::list<ESLRrum>::iterator it = rums.begin (); it != rums.end (); it++)
  {
    if (m_routing.IsLocalRouteAvailable (it->GetDestAddress (), it->GetDestMask ()))
    {
      NS_LOG_LOGIC ("ESLR: Route is about my local network. Skip the RUM");
      continue;
    }
    if (it->GetDbit ()) //if ((it->GetSequenceNo () % 2) != 0)
    {   
      // Managing Poison routes 
      NS_LOG_DEBUG ("ESLR: Invalidating all unresponsive and broken routes.");
      
      invalidatedInBakcup = InvalidateBrokenRoute (it->GetDestAddress (), 
                                                   it->GetDestMask (), 
                                                   senderAddress, eslr::BACKUP);
      invalidatedInMain = InvalidateBrokenRoute (it->GetDestAddress (), 
                                                 it->GetDestMask (), 
                                                 senderAddress, eslr::MAIN);
      continue;
    }
    else
    {
      // calculate LR and SLR cost (i.e., the delay to reach the destination).
      // NOTE:
      //  metric is in the form of microseconds. 
      //  metric is scaled by CCV values
      uint32_t lrCost = CalculateLRCost (m_ipv4->GetNetDevice (incomingInterface));
      uint32_t slrCost = it->GetMatric () + lrCost;

      RoutingTable::RoutesI primaryRoute, secondaryRoute, mainRoute;
      bool foundPrimary, foundSecondary, foundMain;

      // Find the main route
      foundMain = m_routing.FindRouteRecord (it->GetDestAddress (), it->GetDestMask (), mainRoute, eslr::MAIN);
      
      // Find the primary route, which represents the main route
      foundPrimary = m_routing.FindRouteInBackup (it->GetDestAddress (), 
                                                  it->GetDestMask (), 
                                                  primaryRoute, eslr::PRIMARY);
      
      // Find the backup route
      foundSecondary = m_routing.FindRouteInBackup (it->GetDestAddress (), 
                                                    it->GetDestMask (), 
                                                    secondaryRoute, eslr::SECONDARY);
      
      if (!foundPrimary)// && !foundSecondary)
      {
        // No existing routes for the destination. 
        // Add new routes to both tables.
        // specifically settling time is set to zero
        NS_LOG_LOGIC ("ESLR: New network received. Add it to both Main an Backup tables.");

        AddNetworkRouteTo ( it->GetDestAddress (), 
                            it->GetDestMask (), 
                            senderAddress, 
                            incomingInterface, 
                            slrCost, 
                            it->GetSequenceNo (), 
                            eslr::PRIMARY, 
                            eslr::MAIN, 
                            m_routeTimeoutDelay, 
                            m_garbageCollectionDelay, 
                            Seconds (0)); 
        AddNetworkRouteTo ( it->GetDestAddress (), 
                            it->GetDestMask (), 
                            senderAddress, 
                            incomingInterface, 
                            slrCost, 
                            it->GetSequenceNo (), 
                            eslr::PRIMARY, 
                            eslr::BACKUP, 
                            m_routeTimeoutDelay, 
                            m_garbageCollectionDelay, 
                            Seconds (0));                                        
        continue;
      }
      else if (foundPrimary && !foundSecondary)
      {
        NS_LOG_LOGIC ("ESLR: Process the network route" << it->GetDestAddress ());

        // To do: check the logic
        //if ((it->GetSequenceNo () >= primaryRoute->first->GetSequenceNo ()))
        //{
          if ((primaryRoute->first->GetGateway () == senderAddress) && 
							(it->GetSequenceNo () >= primaryRoute->first->GetSequenceNo ()))
          {
            // Update the PRIMARY regardless of the cost of the main route.
            // Nevertheless, at the time Main route expires, 
            // protocol automatically checks for the Primary route.
            // If the primary route's cost is lower that that of the main route, 
            // the main route will be updated according to the primary route. 
            // if ((primaryRoute->first->GetMetric () > slrCost) || 
            //     (primaryRoute->first->GetMetric () == slrCost) || 
            //     (primaryRoute->first->GetMetric () < slrCost))

            UpdateRoute ( it->GetDestAddress (), 
                it->GetDestMask (), 
                senderAddress, 
                incomingInterface, 
                slrCost, 
                it->GetSequenceNo (), 
                eslr::PRIMARY, 
                eslr::BACKUP, 
                m_routeTimeoutDelay, 
                m_garbageCollectionDelay, 
                m_routeSettlingDelay);               
            continue;              
          }
					else if (primaryRoute->first->GetGateway () != senderAddress)
          {
            // Add a secondary route without considering the cost.
            // At the time the Main route get expires, 
            // initially the protocol checks for the secondary route is available.
            // If a low cost secondary route is available, 
            // both Main and the primate routes will be updated according to the the secondary route.
            // if ((primaryRoute->first->GetMetric () > slrCost) || 
            //     (primaryRoute->first->GetMetric () == slrCost) || 
            //     (primaryRoute->first->GetMetric () < slrCost))

            AddNetworkRouteTo ( it->GetDestAddress (), 
                it->GetDestMask (), 
                senderAddress, 
                incomingInterface, 
                slrCost, 
                it->GetSequenceNo (), 
                eslr::SECONDARY, 
                eslr::BACKUP, 
                m_routeTimeoutDelay, 
                m_garbageCollectionDelay, 
                Seconds (0)); 
            continue;
          }
        //}
      }
      else if (foundPrimary && foundSecondary)
      {
        //if ((it->GetSequenceNo () >= primaryRoute->first->GetSequenceNo ()))
        //{
          if ((primaryRoute->first->GetGateway () == senderAddress) &&
							(it->GetSequenceNo () >= primaryRoute->first->GetSequenceNo ()))
          {
            UpdateRoute ( it->GetDestAddress (), 
                it->GetDestMask (), 
                senderAddress, 
                incomingInterface, 
                slrCost, 
                it->GetSequenceNo (), 
                eslr::PRIMARY, 
                eslr::BACKUP, 
                m_routeTimeoutDelay, 
                m_garbageCollectionDelay, 
                m_routeSettlingDelay);               
            continue;               
          }  
          else if ((secondaryRoute->first->GetGateway () == senderAddress) &&
									 (it->GetSequenceNo () >= secondaryRoute->first->GetSequenceNo ()))					
					{
            UpdateRoute ( it->GetDestAddress (), 
                it->GetDestMask (), 
                senderAddress, 
                incomingInterface, 
                slrCost, 
                it->GetSequenceNo (), 
                eslr::SECONDARY, 
                eslr::BACKUP, 
                m_routeTimeoutDelay, 
                m_garbageCollectionDelay, 
                m_routeSettlingDelay);               
            continue;               
          }
          else if ((primaryRoute->first->GetGateway () != senderAddress) && 
                   (secondaryRoute->first->GetGateway () != senderAddress))
          {
            // Make sure that the backup route has the next best cost
            if ((secondaryRoute->first->GetMetric () > slrCost) &&
								(it->GetSequenceNo () >= secondaryRoute->first->GetSequenceNo ()))
            {
              UpdateRoute ( it->GetDestAddress (), 
                  it->GetDestMask (), 
                  senderAddress, 
                  incomingInterface, 
                  slrCost, 
                  it->GetSequenceNo (), 
                  eslr::SECONDARY, 
                  eslr::BACKUP, 
                  m_routeTimeoutDelay, 
                  m_garbageCollectionDelay, 
                  m_routeSettlingDelay);               
              continue;                  
            }
            else
            {
              continue;
            }           
          }
        //}// sequence number is larger that that of the Primary route
      }// both primary and secondary routes are there.
    } // all routes have a valid sequence number
  }
	
	// if invalidated routes found, send an immediate triggered update	
	if (invalidatedInMain)
  {
  	NS_LOG_LOGIC ("ESLR: Invalidated routes in the main table. Send a Triggered update.");

    // As there are disconnected routes, an immediate triggered update is required
    // NOTE: 
    //    In fact, as this is an emergency situation, the cooling time (i.e., 1-5s) is ignored
    //    However, for the protocol management, once a route is determined as broken, the protocol takes
    //    0~2s to mark the route as Disconnected. Therefore, the triggered update has to wait about3 ms.
    if (m_nextTriggeredUpdate.IsRunning ())
        m_nextTriggeredUpdate.Cancel ();
     m_nextTriggeredUpdate = Simulator::Schedule (MilliSeconds (3.), &EslrRoutingProtocol::DoSendRouteUpdate, 
                                               this, eslr::TRIGGERED);          
	}
	if (invalidatedInBakcup)
  {
    NS_LOG_LOGIC ("ESLR: Invalidated backup routes");
  }
}

Ptr<Ipv4Route> 
EslrRoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header << oif);
  
  Ptr<Ipv4Route> rtEntry = 0;
  Ipv4Address destination = header.GetDestination ();
  
  if (destination.IsMulticast ())
  {
    // Note:  Multicast routes for outbound packets are stored in the normal unicast table.
    // This is a well-known property of sockets implementation on many Unix variants.
    // So, just log it and follow the static route search for multicasting
    NS_LOG_LOGIC ("ESLR: Multicast destination");
  }
  
  rtEntry  = LookupRoute (destination, oif);
  
  if (rtEntry)
  {
    NS_LOG_LOGIC ("ESLR: found the route" << rtEntry);  
    sockerr = Socket::ERROR_NOTERROR;
  }
  else
  {
    NS_LOG_LOGIC ("ESLR: no route entry found. Returning the Socket Error");  
    sockerr = Socket::ERROR_NOROUTETOHOST;
  }   
    
  return rtEntry;
}

bool 
EslrRoutingProtocol::RouteInput (Ptr<const Packet> p, 
		const Ipv4Header &header, 
		Ptr<const NetDevice> idev, 
		UnicastForwardCallback ucb, 
		MulticastForwardCallback mcb,
	 	LocalDeliverCallback lcb, 
		ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header << header.GetSource () << header.GetDestination () << idev);
  
  bool retVal = false;
  
  NS_ASSERT (m_ipv4 != 0);
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);
  Ipv4Address dstinationAddress = header.GetDestination ();
  
  if (dstinationAddress.IsMulticast ())
  {
    NS_LOG_LOGIC ("ESLR: Multicast routes are not supported by the ESLR");
    return (retVal = false); // Let other routing protocols try to handle this
  }    
  
  // First find the local interfaces and forward the packet locally.
  // Note: As T. Pecorella mentioned in the RIPng implementation,
  // this method is also checks every interface before forwarding the packet among the local interfaces.
  // However, if we enable the configuration option as mentioned in the \RFC{1222},
  // this forwarding can be done bit intelligently.
  
  for (uint32_t j = 0; j < m_ipv4->GetNInterfaces (); j ++)
  {
    for (uint32_t i = 0; i < m_ipv4->GetNAddresses (j); i ++)
    {
      Ipv4InterfaceAddress iface = m_ipv4->GetAddress (j, i);
      Ipv4Address address = iface.GetLocal ();
    
      if (address.IsEqual (header.GetDestination ()))
      {
        if (j == iif)
        {
          NS_LOG_LOGIC ("ESLR: packet is for me and forwarding it for the interface " << iif);
        }
        else
        {
          NS_LOG_LOGIC ("ESLR: packet is for me but for different interface " << j);
        }
        
        lcb (p, header, iif);
        return (retVal = true);
      }
      
      NS_LOG_LOGIC ("Address " << address << " is not a match");
    }
  }  
  
  // Check the input device supports IP forwarding
  if (m_ipv4->IsForwarding (iif) == false)
  {
    NS_LOG_LOGIC ("ESLR: packet forwarding is disabled for this interface " << iif);
    
    ecb (p, header, Socket::ERROR_NOROUTETOHOST);
    return (retVal = false);
  }

  // Finally, check for route and forwad the packet to the next hop
  NS_LOG_LOGIC ("ESLR: finding a route in the routing table");
  
  Ptr<Ipv4Route> route = LookupRoute (header.GetDestination ()); 
  
  if (route != 0)
  {
    NS_LOG_LOGIC ("ESLR: found a route and calling uni-cast callback");
    ucb (route, p, header);  // uni-cast forwarding callback
    return (retVal = true);
  }
  else
  {
    NS_LOG_LOGIC ("ESLR: no route found");
    return (retVal = false);      
  }
}

Ptr<Ipv4Route>
EslrRoutingProtocol::LookupRoute (Ipv4Address address, Ptr<NetDevice> dev)
{
  NS_LOG_FUNCTION (this << address << dev);
  
  Ptr<Ipv4Route> rtentry = 0;
  bool foundRoute = false;
  
  // Note: if the packet is destined for local multicasting group, 
  // the relevant interfaces has to be specified while looking up the route
  if(address.IsLocalMulticast ())
  {
    NS_ASSERT_MSG (m_ipv4->GetInterfaceForDevice (dev), 
                   "ESLR: destination is for multicasting, and however, no interface index is given!");
    
    rtentry = Create<Ipv4Route> ();
    
    // As the packet is destined to local multicast group, while finding the source address, 
    // the address scope is set as LINK local address
    rtentry->SetSource (m_ipv4->SelectSourceAddress (dev, address, Ipv4InterfaceAddress::LINK)); 
    rtentry->SetDestination (address);
    rtentry->SetGateway (Ipv4Address::GetZero ());
    rtentry->SetOutputDevice (dev);
    
    return rtentry;      
  }
  
  //Now, select a route from the routing table which matches the destination address and its mask
  RoutingTable::RoutesI theRoute;
  foundRoute = m_routing.ReturnRoute (address, dev, theRoute);
  
  if (foundRoute)
  {
    Ipv4RoutingTableEntry* route = theRoute->first;
    uint32_t interfaceIndex = route->GetInterface ();

    rtentry = Create<Ipv4Route> (); 
    
    rtentry->SetDestination (route->GetDest ());
    rtentry->SetGateway (route->GetGateway ());
    rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIndex));  
    
    // As the packet is going to be forwarded in to the next hop, while finding the source address, 
    // the address scope is set as global.          
    rtentry->SetSource (m_ipv4->SelectSourceAddress (m_ipv4->GetNetDevice (interfaceIndex), 
                        route->GetDest (), 
                        Ipv4InterfaceAddress::GLOBAL));

    NS_LOG_DEBUG ("ESLR: found a match for the destination " << 
                  rtentry->GetDestination () << 
                  " via " << rtentry->GetGateway ());  
    
    return rtentry; 
  }
  else
    return rtentry;
}

void 
EslrRoutingProtocol::AddDefaultRouteTo (Ipv4Address nextHop, uint32_t interface)
{
  NS_LOG_FUNCTION (this << nextHop << interface);
    
  AddNetworkRouteTo (Ipv4Address ("0.0.0.0"), 
                     Ipv4Mask::GetZero (), 
                     nextHop, interface, 0, 0, 
                     eslr::PRIMARY, 
                     eslr::MAIN, 
                     Seconds (0), Seconds (0), Seconds (0));
}

void 
EslrRoutingProtocol::AddNetworkRouteTo (Ipv4Address network, 
		Ipv4Mask networkMask, 
		Ipv4Address nextHop, 
		uint32_t interface, 
		uint16_t metric, 
		uint16_t sequenceNo, 
		eslr::RouteType routeType, 
		eslr::Table table, 
		Time timeoutTime, 
		Time garbageCollectionTime, 
		Time settlingTime)
{
  NS_LOG_FUNCTION (this << network << networkMask << nextHop << interface << metric << sequenceNo << routeType);

  RoutingTableEntry* route = new RoutingTableEntry (network, networkMask, nextHop, interface);
  route->SetValidity (eslr::VALID);
  route->SetSequenceNo (sequenceNo);
  route->SetRouteType (routeType);
  route->SetMetric (metric);
  route->SetRouteChanged (true); 

  NS_LOG_LOGIC (this << "ESLR: Add route: " << network << networkMask << ", to " << table); 
  m_routing.AddRoute (route, timeoutTime, garbageCollectionTime, settlingTime, table);        
}

void 
EslrRoutingProtocol::AddNetworkRouteTo (Ipv4Address network, 
		Ipv4Mask networkMask, 
		uint32_t interface, 
		uint16_t metric, 
		uint16_t sequenceNo, 
		eslr::RouteType routeType, 
		eslr::Table table, 
		Time timeoutTime, 
		Time garbageCollectionTime, 
		Time settlingTime)
{
  NS_LOG_FUNCTION (this << network << networkMask << interface << metric << sequenceNo << routeType);

  RoutingTableEntry* route = new RoutingTableEntry (network, networkMask, interface);
  route->SetValidity (eslr::VALID);
  route->SetSequenceNo (sequenceNo);
  route->SetRouteType (routeType);
  route->SetMetric (metric);
  route->SetRouteChanged (true); 

  NS_LOG_LOGIC (this << "ESLR: Add route: " << network << networkMask << ", to " << table);
  
  m_routing.AddHostRoute (route, timeoutTime, garbageCollectionTime, settlingTime, table); 
}

void 
EslrRoutingProtocol::AddHostRouteTo (Ipv4Address host, 
		uint32_t interface, 
		uint16_t metric, 
		uint16_t sequenceNo, 
		eslr::RouteType routeType, 
		eslr::Table table, 
		Time timeoutTime, 
		Time garbageCollectionTime, 
		Time settlingTime)
{
  NS_LOG_FUNCTION (this << host << interface << metric << sequenceNo << routeType);

  // NOTE: 
  //    For host routes of router's local interfaces, 
  //    invalidate time, selling time and garbage collection time 
  //    are specifically set as 0. 
  //    Further, such routes are only add to the main table and those are never expire.

  RoutingTableEntry* route = new RoutingTableEntry (host, interface);

  if (host == "127.0.0.1")
  {
    route->SetValidity (eslr::LHOST); // Neither valid nor invalid
    route->SetRouteChanged (false);     
  }
  else
  {
    route->SetValidity (eslr::VALID);
    route->SetRouteChanged (true); 
  }
  route->SetSequenceNo (sequenceNo);
  route->SetRouteType (routeType);
  route->SetMetric (metric);

  NS_LOG_LOGIC ("ESLR: Add route: " << host << ", to " << table);
  
  m_routing.AddHostRoute (route, timeoutTime, garbageCollectionTime, settlingTime, table); 
}

void 
EslrRoutingProtocol::InvalidateRoutesForInterface (uint32_t interface, eslr::Table table)
{
  NS_LOG_FUNCTION (this << interface );
  
  m_routing.InvalidateRoutesForInterface (interface, 
                                          m_routeTimeoutDelay, 
                                          m_garbageCollectionDelay, 
                                          m_routeSettlingDelay, table);
}

bool 
EslrRoutingProtocol::InvalidateBrokenRoute (Ipv4Address destAddress, Ipv4Mask destMask, Ipv4Address gateway, eslr::Table table)
{
  NS_LOG_FUNCTION (this << destAddress );

  bool retVal = m_routing.InvalidateBrokenRoute (destAddress, 
                                                 destMask, 
                                                 gateway, 
                                                 m_routeTimeoutDelay, 
                                                 m_garbageCollectionDelay, 
                                                 m_routeSettlingDelay, 
                                                 table);
  
  return retVal;
}

void 
EslrRoutingProtocol::UpdateRoute (Ipv4Address network, 
		Ipv4Mask networkMask, 
		Ipv4Address nextHop, 
		uint32_t interface, 
		uint16_t metric, 
		uint16_t sequenceNo, 
		eslr::RouteType routeType, 
		eslr::Table table, 
		Time timeoutTime, 
		Time garbageCollectionTime, 
		Time settlingTime)
{
  NS_LOG_FUNCTION (this << network << networkMask << nextHop << interface << metric << sequenceNo << routeType);

  RoutingTableEntry* route = new RoutingTableEntry (network, networkMask, nextHop, interface);
  route->SetValidity (eslr::VALID);
  route->SetSequenceNo (sequenceNo);
  route->SetRouteType (routeType);
  route->SetMetric (metric);
  route->SetRouteChanged (true); 

  NS_LOG_DEBUG ("ESLR: Add route: " << network << networkMask << ", to " << table); 
  m_routing.UpdateNetworkRoute (route, timeoutTime, garbageCollectionTime, settlingTime, table);        
}

Ptr<Socket> 
EslrRoutingProtocol::GetSocketForInterface (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);

  Ptr<Socket> sock = 0;
  if (!m_sendSocketList.empty ())    
  {
    for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
    {
      if (iter->second == interface)
      {
         return (sock = iter->first);
      }
    }
  }
  return (sock = 0);
}

int32_t
EslrRoutingProtocol::GetInterfaceForSocket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  int32_t interface = -1;
  if (!m_sendSocketList.empty ())    
  {
    for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
    {
      if (iter->first == socket)
      {
         return (interface = iter->second);
      }
    }
  }
  return (interface = -1);
}

uint32_t 
EslrRoutingProtocol::genarateNeighborID ()
{
  Ptr<Node> theNode = GetObject<Node> ();
  return m_nodeId = theNode->GetId ();
}

void 
EslrRoutingProtocol::SetInterfaceExclusions (std::set<uint32_t> exceptions)
{
  NS_LOG_FUNCTION (this);

  m_interfaceExclusions = exceptions;    
}

uint32_t 
EslrRoutingProtocol::CalculateLRCost (Ptr<NetDevice> dev)
{
  Ptr<Node> node = m_ipv4->GetObject<Node> ();
  
  double transDelay = 0.0, propagationDelay = 0.0, LCost = 0.0;
  double RCost = 0.0, LRCost = 0.0;
  if (m_K2 != 0)
	{
  	GetLinkDetails (dev, transDelay, propagationDelay);
  	// the delay that a packet takes to reach to its other end
  	LCost = transDelay + propagationDelay;
	}
	if (m_K3 != 0)
	{
  	RCost = (1 / (node->GetRouterMue () - node->GetRouterLambda ())) * 1000; // in Milliseconds
	}
	
	// Scale-up the metric using Scaling factor of CCVs
  LRCost = (m_K2 * LCost + m_K3 * RCost); // in Milliseconds

  return uint32_t (LRCost);
}

void
EslrRoutingProtocol::GetLinkDetails (Ptr<NetDevice> dev, double &transDelay, double &propagationDelay)
{
  NS_LOG_FUNCTION (this<<dev);
	NS_ASSERT_MSG((dev != 0),"Check the NetDevice");  
	
  Ptr<Node> node = m_ipv4->GetObject<Node> ();
  
  double linkBandwidth = 0.0, availableBW = 0.0;  
  uint32_t totalBitsInLink = 0, previousValue = 0, tempValue = 0;
    
  // Get Channel Attributes
  StringValue getDelay, getBW;
  std::string temp, delay, bandwidth;
  
  Ptr<Channel> channel = dev->GetChannel ();
  uint8_t nDevices = channel->GetNDevices ();
  
  channel->GetAttribute ("Delay", getDelay);
  delay = getDelay.Get ().c_str ();

  temp = delay.substr (1, (delay.size () - 5));
  
  propagationDelay = (double)(atof (temp.c_str ())/1000000.0); // Converted to MilliSeconds as it comes with ns
  
  // Get the capacity of the link
  dev->GetAttribute("DataRate",getBW);
  bandwidth = getBW.Get ().c_str ();

  temp = bandwidth.substr (0, (bandwidth.size () - 3));
  linkBandwidth = (double) (atof (temp.c_str ())); // in bps
  
  // now calculate the available bandwidth of the channel (entire link) 
  Ptr<NetDevice> devList [nDevices];
  for (uint8_t i = 0; i < nDevices; i++)
  {
    devList [i] = channel->GetDevice (i);
    totalBitsInLink =+ (node->GetAveragePacketSizeOfDevice (channel->GetDevice (i)) *
                        node->GetNofPacketsOfDevice (channel->GetDevice (i)) *
                        8);    
  } 
  
  // calculate link occupancy.
  // However, this calculation is bit ambiguity. Has to be fixed.
  // TODO
  tempValue = totalBitsInLink - previousValue;
  previousValue = totalBitsInLink;
  
  if (tempValue < 0)
    tempValue = 0;
  // Available bandwidth of the link 
  availableBW = linkBandwidth - tempValue;
  
  //based on the available bandwidth, the packet propagation time
  transDelay = ((node->GetAveragePacketSizeOfDevice (dev) * 8) / availableBW ) * 1000; // in Milliseconds
}

void
EslrRoutingProtocol::PrintStats ()
{
  NS_LOG_FUNCTION (this);

  std::cout << int(m_nodeId) << ":" << m_protocolMessages << std::endl;
  m_protocolMessages =0; // reset the counter.
  m_countingEvent = Simulator::Schedule (m_printDuration, &EslrRoutingProtocol::PrintStats, this);  
}

}// end of namespace eslr
}// end of namespave ns3

