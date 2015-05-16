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

#include "eslr-main.h"

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
                                              m_neighborTable ()
{
  m_rng = CreateObject<UniformRandomVariable> ();

  //  for testing purposes
  //  AddNetworkRouteTo (Ipv4Address (), Ipv4Mask (), "11.118.126.1", 1, 10, 2, eslr::SECONDARY, eslr::BACKUP, Seconds (50), Seconds (20), Seconds (100));

  //  AddNetworkRouteTo (Ipv4Address (), Ipv4Mask (), "212.23.25.10", 6, 15, 6, eslr::SECONDARY, eslr::BACKUP, Seconds (500), Seconds (20), Seconds (0));
}

EslrRoutingProtocol::~EslrRoutingProtocol () {/*destructor*/}

TypeId EslrRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::eslr::EslrRoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .AddConstructor<EslrRoutingProtocol> ()
    .AddAttribute ( "KeepAliveInterval","The time between two Keep Alive Messages.",
			              TimeValue (Seconds(30)), /*This has to be adjust according to the user requirment*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_kamTimer),
			              MakeTimeChecker ())
    .AddAttribute ( "NeighborTimeoutDelay","The delay to mark a neighbor as unresponsive.",
			              TimeValue (Seconds(90)), /*This has to be adjust according to the user requirment*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_neighborTimeoutDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "GarbageCollection","The delay to remove unresponsive neighbors from the neighbor table.",
			              TimeValue (Seconds(20)), /*This has to be adjust according to the user requirment*/
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
			              TimeValue (Seconds(180)), /*This has to be adjust according to the user requirment*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_routeTimeoutDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "SettlingTime","The delay that a route record has to keep in the backup table before it is moved to the main table.",
			              TimeValue (Seconds(60)), /*This has to be adjust according to the user requirment*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_routeSettlingDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "MinTriggeredCooldown","Minimum time gap between two triggered updates.",
			              TimeValue (Seconds(1)), /*This has to be adjust according to the user requirment*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_minTriggeredCooldownDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "MaxTriggeredCooldown","Maximum time gap between two triggered updates.",
			              TimeValue (Seconds(5)), /*This has to be adjust according to the user requirment*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_maxTriggeredCooldownDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "PeriodicUpdateInterval","Time between two periodic updates.",
			              TimeValue (Seconds(30)), /*This has to be adjust according to the user requirment*/
			              MakeTimeAccessor (&EslrRoutingProtocol::m_periodicUpdateDelay),
			              MakeTimeChecker ())
    .AddAttribute ( "PrintingMethod", "Specify which table has to be print.",
                    EnumValue (DONT_PRINT),
                    MakeEnumAccessor (&EslrRoutingProtocol::m_print),
                    MakeEnumChecker ( MAIN_R_TABLE, "MainRoutingTable",
                                      N_TABLE, "NeighborTable",
                                      BACKUP_R_TABLE, "BackupRoutingTable"))
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

  m_initialized = true; // Indicate that the routing protocol is initialized

  m_routing.AssignStream (m_stream);

  // build the socket interface list
  // The interface ID = 0 is not considered in this implementation
  // 0th interface is always the loop back interface "127.0.0.1". Therefore, for the route management, the interface 0 is Purposely omited
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
        NS_LOG_LOGIC ("ESLR: adding sending socket to " << iface.GetLocal ());
    
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
    NS_LOG_LOGIC ("ESLR: adding receiving socket");
      
    m_recvSocket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
    NS_ASSERT (m_recvSocket != 0);

    m_recvSocket->Bind (InetSocketAddress(Ipv4Address::GetAny (), ESLR_MULT_PORT));
    //m_recvSocket->SetIpTtl (1);
    m_recvSocket->SetIpRecvTtl (true);
    m_recvSocket->SetRecvCallback (MakeCallback (&EslrRoutingProtocol::Receive,this));
    m_recvSocket->SetRecvPktInfo (true);
  }

  // Schedule to send the Hello/KAM messages to discove and initiate the neighbor tables
  Time delay = Seconds (m_rng->GetValue (0.1, 0.01*m_kamTimer.GetSeconds ()));
  m_nextKeepAliveMessage = Simulator::Schedule (delay, &EslrRoutingProtocol::sendKams, this);

  // If there are newly added routes, schedule a triggered update
  if (addedGlobal)
  {
    delay = Seconds (m_rng->GetValue (m_minTriggeredCooldownDelay.GetSeconds (), m_maxTriggeredCooldownDelay.GetSeconds ()));
    m_nextTriggeredUpdate = Simulator::Schedule (delay, &EslrRoutingProtocol::DoSendRouteUpdate, this, eslr::TRIGGERED);
  }

  // Otherwise schedule a periodic update
  delay = m_periodicUpdateDelay + Seconds (m_rng->GetValue (0, 0.5*m_periodicUpdateDelay.GetSeconds ()));
  m_nextPeriodicUpdate = Simulator::Schedule (delay, &EslrRoutingProtocol::SendPeriodicUpdate, this);

  Ipv4RoutingProtocol::DoInitialize ();
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

Ptr<Ipv4Route> 
EslrRoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,Socket::SocketErrno &sockerr)
{
  Ptr<Ipv4Route> rtEntry = 0;
  return rtEntry;
}

bool 
EslrRoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
  bool retVal = false;
  return retVal;
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
        AddHostRouteTo (iface.GetLocal(), 0, 0, 0, eslr::PRIMARY, eslr::MAIN, Seconds (0), Seconds (0), Seconds (0));
      }
      else
      {
        AddNetworkRouteTo (ifaceNetworkAddress, ifaceNetMask, interface, 0, 0, eslr::PRIMARY, eslr::MAIN, Seconds (0), Seconds (0), Seconds (0));
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
    Ipv4Address ifaceNetworkAddress = iface.GetLocal().CombineMask(ifaceNetMask);  
    
    if (iface.GetScope() == Ipv4InterfaceAddress::GLOBAL && 
        foundSendSocket == false && 
        activeInterface == true)
    {
      NS_LOG_LOGIC ("ESLR: adding sending socket to " << iface.GetLocal ());
      
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
      NS_ASSERT (socket != 0);

      socket->Bind (InetSocketAddress(iface.GetLocal (),ESLR_BROAD_PORT));
      socket->BindToNetDevice (m_ipv4->GetNetDevice (interface));

      //socket->SetIpTtl (1);
      socket->SetIpRecvTtl (true);
      socket->SetAllowBroadcast (true);
      socket->SetRecvCallback (MakeCallback (&EslrRoutingProtocol::Receive,this));
      socket->SetRecvPktInfo (true);

      NS_LOG_LOGIC ("ESLR: add the socket to the socket list " << iface.GetLocal ());

      m_sendSocketList[socket] = interface;
    }
  }
  
  if (!m_recvSocket)
  {
    NS_LOG_LOGIC ("ESLR: adding receiving socket");
      
    m_recvSocket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
    NS_ASSERT (m_recvSocket != 0);

    m_recvSocket->Bind (InetSocketAddress(Ipv4Address::GetAny (), ESLR_MULT_PORT));
    //m_recvSocket->SetIpTtl (1);
    m_recvSocket->SetIpRecvTtl (true);
    m_recvSocket->SetRecvCallback (MakeCallback (&EslrRoutingProtocol::Receive,this));
    m_recvSocket->SetRecvPktInfo (true);
  }

SendTriggeredRouteUpdate ();
}

void 
EslrRoutingProtocol::NotifyInterfaceDown (uint32_t interface)
{
  NS_LOG_FUNCTION (this << interface);
  
  InvalidateRoutesForInterface (interface, eslr::BACKUP);
  InvalidateRoutesForInterface (interface, eslr::MAIN);

  //To do: invalidate neighbors for the interface

  for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
  {
    NS_LOG_LOGIC ("Checking socket for interface " << interface);

    if (iter->second == interface)
    {
      NS_LOG_LOGIC ("Removed socket for interface " << interface);

      iter->first->Close ();
      m_sendSocketList.erase (iter);
      break;
    }
  }

  if (m_interfaceExclusions.find (interface) == m_interfaceExclusions.end ())
  {
    SendTriggeredRouteUpdate ();
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
    AddNetworkRouteTo (networkAddress, netMask, interface, 0, 0, eslr::PRIMARY, eslr::MAIN, Seconds (0), Seconds (0), Seconds (0));
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

void EslrRoutingProtocol::sendKams ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> p = Create<Packet> ();
  SocketIpTtlTag tag;

  //p->RemovePacketTag (tag);
  tag.SetTtl (1);
  p->AddPacketTag (tag);
  
  // Note:In this method, we assume that number of iterfaces are not exceeding Maximum RUMs that a 
  //      ESLRRouting header supports. Therefore, we do not consider of calculating the number of 
  //      RUMs that include in to a ESLRouting header. 
  //      However, following equation can be used to calculate maximum number of RUMs 
  //      that can be added to a ESLR routing header
  //
  // uint16_t mtu = m_ipv4->GetMtu (interface);
  // uint16_t maxRum = (mtu - Ipv4Header ().GetSerializedSize () - UdpHeader ().GetSerializedSize () - ESLRRoutingHeader ().GetSerializedSize ()) / ESLRrum ().GetSerializedSize ();

  ESLRRoutingHeader hdr;
  hdr.SetCommand (eslr::KAM);
  hdr.SetRuCommand (eslr::NO);
  hdr.SetRoutingTableRequestType (eslr::NON);
  hdr.SetAuthType (PLAIN_TEXT); // User should enter auth type
  hdr.SetAuthData (1234); // User should enter Auth data
  
	for (SocketListI iter = m_sendSocketList.begin (); iter != m_sendSocketList.end (); iter++ )
	{
    KAMHeader kamHdr;
    uint32_t interface = iter->second;
  
    kamHdr.SetCommand (eslr::HELLO);
    kamHdr.SetAuthType (eslr::PLAIN_TEXT);
    kamHdr.SetAuthData (1234);
    kamHdr.SetNeighborID (genarateNeighborID ());
    
    // Get the interface attributes
    Ipv4InterfaceAddress iface = m_ipv4->GetAddress (interface, 0);
    Ipv4Address interfaceAddress= iface.GetLocal ();
    Ipv4Mask interfaceNetMask = iface.GetMask ();

    kamHdr.SetGateway (interfaceAddress);
    kamHdr.SetGatewayMask (interfaceNetMask);

    // Set & send the packet
    hdr.AddKam (kamHdr);
    p->AddHeader (hdr);
    if (m_interfaceExclusions.find (interface) == m_interfaceExclusions.end ())
    {
      NS_LOG_LOGIC ("SendTo: " << *p);
      
      // Send KAM updates
      iter->first->SendTo (p, 0, InetSocketAddress (iface.GetBroadcast (), ESLR_BROAD_PORT));	
      p->RemoveHeader (hdr);
      hdr.ClearKams ();
    }
	}		
  
  // Reschedule next KAM
	Time sendKam = Seconds (m_rng->GetValue (0, 0.5*m_kamTimer.GetSeconds ()));
  m_nextKeepAliveMessage = Simulator::Schedule (sendKam, &EslrRoutingProtocol::sendKams, this);
}

void 
EslrRoutingProtocol::SendRouteRequest ()
{
  //tbi
}

void 
EslrRoutingProtocol::SendTriggeredRouteUpdate ()
{
  NS_LOG_FUNCTION (this);

  if (m_nextTriggeredUpdate.IsRunning())
  {
    NS_LOG_LOGIC ("Skipping Triggered Update due to cooldown");
    return;
  }

  // Note:  This part id directly inherited from the RFC 2080
  //        After a triggered update is sent, a timer is set between 1s to 5s. 
  //        between that time, any other triggered updates are not sent. 
  //        In addition, triggered updates are omitted if there is a scheduled periodic update.
  //        Furthermore, only changed routes will be sent. 
  //        Rest of the routes are not considered as a RUM for the update message. 
  //        Thus, after sending either triggered or periodic update, the changed flag will be set to false 

  Time delay = Seconds (m_rng->GetValue (m_minTriggeredCooldownDelay.GetSeconds (), m_maxTriggeredCooldownDelay.GetSeconds ()));
  m_nextTriggeredUpdate = Simulator::Schedule (delay, &EslrRoutingProtocol::DoSendRouteUpdate, this, eslr::TRIGGERED);
}

void 
EslrRoutingProtocol::SendPeriodicUpdate (void)
{
  NS_LOG_FUNCTION (this);    

  if (m_nextTriggeredUpdate.IsRunning ())
    m_nextTriggeredUpdate.Cancel ();

  DoSendRouteUpdate (eslr::PERIODIC);

  Time delay = m_periodicUpdateDelay + Seconds (m_rng->GetValue (0, 0.5*m_periodicUpdateDelay.GetSeconds ()));
  m_nextPeriodicUpdate = Simulator::Schedule (delay, &EslrRoutingProtocol::SendPeriodicUpdate, this);
}

void 
EslrRoutingProtocol::DoSendRouteUpdate (eslr::UpdateType updateType)
{
  NS_LOG_FUNCTION (this);

  if (updateType == eslr::PERIODIC)
  {
    NS_LOG_LOGIC (this << "Periodic Update");
  }
  else if (updateType == eslr::TRIGGERED)  
  {
    NS_LOG_LOGIC (this << "Triggered Update");
  }

  // acquiring an instance of the neighbor table
  NeighborTable::NeighborTableInstance tempNeighbor;
  m_neighborTable.ReturnNeighborTable (tempNeighbor);
  NeighborTable::NeighborI it;

  // acquiring an instance of the main routing table
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
      uint16_t maxRum = (mtu - Ipv4Header ().GetSerializedSize () - UdpHeader ().GetSerializedSize () - ESLRRoutingHeader ().GetSerializedSize ()) / ESLRrum ().GetSerializedSize ();

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
        bool splitHorizoning = (rtIter->first->GetInterface () == interface);

        bool isLocalHost = ((rtIter->first->GetDestNetwork () == "127.0.0.1") && (rtIter->first->GetDestNetworkMask () == Ipv4Mask::GetOnes ()));

        // Note:  all splithorizon routes are omited.
        //        routes about the local host is omited.
        //        only changed routes are considered.
        if ((m_splitHorizonStrategy != (SPLIT_HORIZON && splitHorizoning)) && 
            (!isLocalHost) &&
            (updateType == eslr::PERIODIC || rtIter->first->GetRouteChanged ()))
        { 
          ESLRrum rum;

          if (rtIter->first->GetValidity () == eslr::VALID)
            rum.SetSequenceNo (rtIter->first->GetSequenceNo () + 2);
          else if (rtIter->first->GetValidity () == eslr::DISCONNECTED)
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
          
          //it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (ESLR_MULT_ADD, ESLR_MULT_PORT));
          it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (iface.GetBroadcast (), ESLR_MULT_PORT));
          p->RemoveHeader (hdr);
          hdr.ClearRums ();
        }
      }
      if (hdr.GetNoe () > 0)
      {
        p->AddHeader (hdr);
        NS_LOG_LOGIC ("SendTo: " << *p);
     
        //it->first->GetSocket ()->SendTo (p, 0, InetSocketAddress (ESLR_MULT_ADD, ESLR_MULT_PORT));
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
}

void 
EslrRoutingProtocol::Receive (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet = socket->Recv ();

  NS_LOG_LOGIC ("Received " << *packet);

  Ipv4PacketInfoTag interfaceInfo;
  if (!packet->RemovePacketTag (interfaceInfo))
  {
    NS_ABORT_MSG ("No TTL tag information attached for ESLR message, aborting.");
  }

  SocketIpTtlTag TtlInfoTag;
  if (!packet->RemovePacketTag (TtlInfoTag))
  {
    NS_ABORT_MSG ("No incoming interface on ESLR message, aborting.");
  }

  SocketAddressTag tag;
  if (!packet->RemovePacketTag (tag))
  {
    NS_ABORT_MSG ("No incoming sender address on ESLR message, aborting.");
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
    NS_LOG_LOGIC ("A piggybacked packet, Ignoring it.");
    return;
  }

  NS_LOG_LOGIC ("Handle the request packet.");
  
  ESLRRoutingHeader hdr;
  packet->RemoveHeader (hdr);

  if (hdr.GetCommand () == eslr::KAM)
  {
    // No security is considered in this phase
    NS_LOG_LOGIC ("Handle the KAM packet.");

    // Get the actual bounded socket for the received interface.
    // Multicast addresses are bound to an another public socket
    // However, Neighbors should represent the actual received interface index and 
    // the socket bounded to the interface.
    // neighbor table maintains the socket and interface combination
		ipInterfaceIndex = GetInterfaceForSocket (socket);

  	if (ipInterfaceIndex == -1)
  	{
    	NS_ABORT_MSG ("No incoming interface on ESLR message, aborting.");
  	} 

    HandleKamRequests (hdr, senderAddress, ipInterfaceIndex);
  }
  else if (hdr.GetCommand () == eslr::RU)
  {
    // Route update authentication is not considered.
    // As multicasting route update messages are receiving from non neighbor routers, 
    // it is not possible to use per neighbor authentication. 
    // Thererore, per neighbor authentication is temporarily disabled.
    // Every packet are accepted.
    // However, in this case, we have to consider some method to prevent DDoS attacks 

    //NeighborTable::NeighborI neighborRecord;
    //bool isNeighborPresent = m_neighborTable.FindValidNeighborForAddress (senderAddress, neighborRecord);
    //if (isNeighborPresent)
    //{
    //   handle route request messages     
    //}
    //else
    //{
    //  NS_ABORT_MSG ("Sender is not a neighbor of me, aborting.");
    //}

    if (hdr.GetRuCommand () == eslr::REQUEST)
    {
      HandleRouteRequests (hdr, senderAddress, senderPort, ipInterfaceIndex);
    }
    else if (hdr.GetRuCommand () == eslr::RESPONSE)
    {
      HandleRouteResponses (hdr, senderAddress, ipInterfaceIndex);        
    }
  }
  else if (hdr.GetCommand () == eslr::SRC)
  {
    // To do: Implement;
  }
  else
  {
    NS_LOG_LOGIC ("Ignoring message with unknown command: " << int (hdr.GetCommand ()));
  }
  return;
}

void 
EslrRoutingProtocol::HandleKamRequests (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface)
{
  NS_LOG_FUNCTION (this << senderAddress <<  incomingInterface  << hdr);

  std::list<KAMHeader> kams = hdr.GetKamList ();

  if (kams.empty ())
  {
    NS_LOG_LOGIC (this << "No Keep Alive Messages attached.");
    return;
  }

  if (m_interfaceExclusions.find (incomingInterface) == m_interfaceExclusions.end ())
  {
    // Assuming that future version of the KAMs contain multiple messages,
    // This method is implemented.
    for (std::list<KAMHeader>::iterator iter = kams.begin (); iter != kams.end (); iter++)
    {
      NeighborTable::NeighborI neighborRecord;

      Ptr<Socket> receivedSocket = GetSocketForInterface (incomingInterface);

      if (receivedSocket == 0)
      {
        NS_ABORT_MSG ("No matching socket found for the incoming interface, aborting.");
      }

      bool isNeighborPresent = m_neighborTable.FindNeighbor (iter->GetNeighborID (), neighborRecord);

      if (!isNeighborPresent)
      {
        NS_LOG_LOGIC ("Adding the Neighbor" <<  iter->GetNeighborID () << iter->GetGateway ());

        NeighborTableEntry* newNeighbor = new  NeighborTableEntry ( iter->GetNeighborID (), iter->GetGateway (),
                                                                    iter->GetGatewayMask (), incomingInterface,
                                                                    receivedSocket, iter->GetAuthType (),
                                                                    iter->GetAuthData (), eslr::VALID);
                                                                    
        m_neighborTable.AddNeighbor ( newNeighbor, m_neighborTimeoutDelay, 
                                      m_garbageCollectionDelay, m_routing, 
                                      m_routeTimeoutDelay, m_garbageCollectionDelay, 
                                      m_routeSettlingDelay);
			}
      else
			{
        NS_LOG_DEBUG ("Updating the Neigbor" <<  neighborRecord->first->GetNeighborID () << neighborRecord->first->GetNeighborAddress ());

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
                                        m_garbageCollectionDelay, m_routing);	

        // To do: Ask initial routes
      }
    }
  }
}

void 
EslrRoutingProtocol::HandleRouteRequests (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint16_t senderPort, uint32_t incomingInterface)
{
  // Todo
}

void 
EslrRoutingProtocol::HandleRouteResponses (ESLRRoutingHeader hdr, Ipv4Address senderAddress, uint32_t incomingInterface)
{
  NS_LOG_FUNCTION (this << senderAddress << incomingInterface << hdr);

  if (m_interfaceExclusions.find (incomingInterface) != m_interfaceExclusions.end ())
  {
    NS_LOG_DEBUG ("Ignoring an update message from an excluded interface: " << incomingInterface);
    return;
  }
  
  std::list<ESLRrum> rums = hdr.GetRumList ();

  NS_LOG_DEBUG ("Invalidating all unresponsive and broken routes.");

  for (std::list<ESLRrum>::iterator it = rums.begin (); it != rums.end (); it++)
  {
    if ((it->GetSequenceNo () % 2) != 0)
    {
      bool invalidatedInBakcup = InvalidateBrokenRoute (it->GetDestAddress (), it->GetDestMask (), senderAddress, eslr::BACKUP);
      bool invalidatedInMain = InvalidateBrokenRoute (it->GetDestAddress (), it->GetDestMask (), senderAddress, eslr::MAIN);

      if (invalidatedInMain)
      {
        NS_LOG_LOGIC ("Invalidating routes in the main table. Send a Triggered update.");

        // As there are invalidated routes, an immediate triggered update is triggered
        // Note: However, Triggered updates are frequently called, the compensating time (1-5s)
        // is affected
        if (m_nextTriggeredUpdate.IsRunning ())
          m_nextTriggeredUpdate.Cancel ();
        SendTriggeredRouteUpdate ();
      }

      if (invalidatedInBakcup)
      {
        NS_LOG_LOGIC ("Invalidating backup routes");
      }
      continue;
    }
    else
    {
      if (m_routing.IsLocalRouteAvailable (it->GetDestAddress (), it->GetDestMask ()))
      {
        NS_LOG_LOGIC ("Route is about my local network. Skip the RUM");
        continue;
      }

      // To do: write methods to calculate LR cost
      // for now, all routes will get a random cost between 1-20 at the receive

      uint16_t lrCost = m_rng->GetValue (1,10);
      uint16_t slrCost = it->GetMatric () + lrCost;

      RoutingTable::RoutesI primaryRoute, secondaryRoute, mainRoute;
      bool foundPrimary, foundSecondary, foundMain;

      foundMain = m_routing.FindRouteRecord (it->GetDestAddress (), it->GetDestMask (), mainRoute, eslr::MAIN);
      foundPrimary = m_routing.FindRouteInBackup (it->GetDestAddress (), it->GetDestMask (), primaryRoute, eslr::PRIMARY);
      foundSecondary = m_routing.FindRouteInBackup (it->GetDestAddress (), it->GetDestMask (), secondaryRoute, eslr::SECONDARY);
      
      if (!foundPrimary && !foundSecondary)
      {
        // No existing routes for the destination. 
        // Add a new route.
        // Initially routes are added to the the backup table.
        // after expiring the settling time, routes will be moved to the main routing table
        // The route status will change as primary (on both Main and Backup tables).
        NS_LOG_LOGIC ("New network received. Add it to both Main an Backup tables.");

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
        NS_LOG_LOGIC (this << "Process the network route" << it->GetDestAddress ());

        if ((it->GetSequenceNo () >= primaryRoute->first->GetSequenceNo ()))
        {
          if (primaryRoute->first->GetGateway () == senderAddress)
          {
            // Update the PRIMARY route without considering the cost
            // At the time Main route expires, protocol automatically checks for the Primary route.
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
          if (primaryRoute->first->GetGateway () != senderAddress)
          {
            // Add a secondary route without considering the cost.
            // At the time the Main route get expires, 
            // initially the protocol checks for the secondary route is available.
            // If a low cost secondary route is available, 
            // both Main and the primaty routes are updated according to the the secondary route.
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
        }
      }
      else if (foundPrimary && foundSecondary)
      {
        if ((it->GetSequenceNo () >= primaryRoute->first->GetSequenceNo ()))
        {
          if (primaryRoute->first->GetGateway () == senderAddress)
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
          else if (secondaryRoute->first->GetGateway () == senderAddress)
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
            if (secondaryRoute->first->GetMetric () > slrCost)
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
        }// sequence number is larger that that of the Primary route
      }// both primary and secondary routes are there.
    } // all routes have a valid sequence number
  }    
}

void 
EslrRoutingProtocol::AddDefaultRouteTo (Ipv4Address nextHop, uint32_t interface)
{
  NS_LOG_FUNCTION (this << nextHop << interface);
    
  AddNetworkRouteTo (Ipv4Address ("0.0.0.0"), Ipv4Mask::GetZero (), nextHop, interface, 0, 0, eslr::PRIMARY,  eslr::MAIN, Seconds (0), Seconds (0), Seconds (0));
}

void 
EslrRoutingProtocol::AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, Ipv4Address nextHop, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime)
{
  NS_LOG_FUNCTION (this << network << networkMask << nextHop << interface << metric << sequenceNo << routeType);

  RoutingTableEntry* route = new RoutingTableEntry (network, networkMask, nextHop, interface);
  route->SetValidity (eslr::VALID);
  route->SetSequenceNo (sequenceNo);
  route->SetRouteType (routeType);
  route->SetMetric (metric);
  route->SetRouteChanged (true); 

  NS_LOG_LOGIC (this << "Add route: " << network << networkMask << ", to " << table); 
  m_routing.AddRoute (route, timeoutTime, garbageCollectionTime, settlingTime, table);        
}

void 
EslrRoutingProtocol::AddNetworkRouteTo (Ipv4Address network, Ipv4Mask networkMask, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime)
{
  NS_LOG_FUNCTION (this << network << networkMask << interface << metric << sequenceNo << routeType);

  RoutingTableEntry* route = new RoutingTableEntry (network, networkMask, interface);
  route->SetValidity (eslr::VALID);
  route->SetSequenceNo (sequenceNo);
  route->SetRouteType (routeType);
  route->SetMetric (metric);
  route->SetRouteChanged (true); 

  NS_LOG_LOGIC (this << "Add route: " << network << networkMask << ", to " << table);
  
  m_routing.AddHostRoute (route, timeoutTime, garbageCollectionTime, settlingTime, table); 
}

void 
EslrRoutingProtocol::AddHostRouteTo (Ipv4Address host, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime)
{
  NS_LOG_FUNCTION (this << host << interface << metric << sequenceNo << routeType);

  // Note: For host routes of router's local interfaces, 
  // invalidate time, selling time and garbage collection time 
  // are specifically set as 0. 
  // Further, such routes are only add to the main table and those are never expire.

  RoutingTableEntry* route = new RoutingTableEntry (host, interface);

  if (host == "127.0.0.1")
    route->SetValidity (eslr::LHOST); // Neither valid nor invalid
  else
    route->SetValidity (eslr::VALID);

  route->SetSequenceNo (sequenceNo);
  route->SetRouteType (routeType);
  route->SetMetric (metric);
  route->SetRouteChanged (true); 

  NS_LOG_LOGIC (this << "Add route: " << host << ", to " << table);
  
  m_routing.AddHostRoute (route, timeoutTime, garbageCollectionTime, settlingTime, table); 
}

void 
EslrRoutingProtocol::InvalidateRoutesForInterface (uint32_t interface, eslr::Table table)
{
  NS_LOG_FUNCTION (this << "Find and delete routes for interface: " << interface );
  m_routing.InvalidateRoutesForInterface (interface, m_routeTimeoutDelay, m_garbageCollectionDelay, m_routeSettlingDelay);
}

bool 
EslrRoutingProtocol::InvalidateBrokenRoute (Ipv4Address destAddress, Ipv4Mask destMask, Ipv4Address gateway, eslr::Table table)
{
  NS_LOG_FUNCTION (this << destAddress );

  bool retVal = m_routing.InvalidateBrokenRoute (destAddress, destMask, gateway, m_routeTimeoutDelay, m_garbageCollectionDelay, m_routeSettlingDelay, table);
  return retVal;
}

void 
EslrRoutingProtocol::UpdateRoute (Ipv4Address network, Ipv4Mask networkMask, Ipv4Address nextHop, uint32_t interface, uint16_t metric, uint16_t sequenceNo, eslr::RouteType routeType, eslr::Table table, Time timeoutTime, Time garbageCollectionTime, Time settlingTime)
{
  NS_LOG_FUNCTION (this << network << networkMask << nextHop << interface << metric << sequenceNo << routeType);

  RoutingTableEntry* route = new RoutingTableEntry (network, networkMask, nextHop, interface);
  route->SetValidity (eslr::VALID);
  route->SetSequenceNo (sequenceNo);
  route->SetRouteType (routeType);
  route->SetMetric (metric);
  route->SetRouteChanged (true); 

  NS_LOG_DEBUG (this << "Add route: " << network << networkMask << ", to " << table); 
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

}// end of namespace eslr
}// end of namespave ns3

