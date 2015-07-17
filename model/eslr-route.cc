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

#include "eslr-route.h"

#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/node-list.h"
#include "ns3/node.h"
#include "ns3/timer.h"


NS_LOG_COMPONENT_DEFINE ("ESLRRouteTable");

namespace ns3 {
namespace eslr {

/* 
* Routing Table Entry
*/

RoutingTableEntry::RoutingTableEntry (void) : Ipv4RoutingTableEntry (RoutingTableEntry::CreateNetworkRouteTo
(Ipv4Address (), Ipv4Mask (), Ipv4Address (), 0)), m_sequenceNo (0),
                                            m_metric (0),
                                            m_changed (false),
                                            m_validity (eslr::INVALID),
                                            m_routeType (eslr::SECONDARY),
                                            m_routeTag (0)
{
  /*cstrctr*/
}

RoutingTableEntry::RoutingTableEntry (Ipv4Address network, 
                                        Ipv4Mask networkMask, 
                                        Ipv4Address nextHop, 
                                        uint32_t interface) :
                                        Ipv4RoutingTableEntry (RoutingTableEntry::CreateNetworkRouteTo
(network, networkMask, nextHop, interface)), m_sequenceNo (0),
                                           m_metric (0),
                                           m_changed (false),
                                           m_validity (eslr::INVALID),
                                           m_routeType (eslr::SECONDARY),
                                           m_routeTag (0)
{
  /*cstrctr*/
}

RoutingTableEntry::RoutingTableEntry (Ipv4Address network, 
                                        Ipv4Mask networkMask,
                                        uint32_t interface) :
                                        Ipv4RoutingTableEntry (RoutingTableEntry::CreateNetworkRouteTo
(network, networkMask, interface)), m_sequenceNo (0),
                                  m_metric (0),
                                  m_changed (false),
                                  m_validity (eslr::INVALID),
                                  m_routeType (eslr::SECONDARY),
                                  m_routeTag (0)
{
  /*cstrctr*/
}

RoutingTableEntry::RoutingTableEntry (Ipv4Address host,
                                        uint32_t interface) :
                                        Ipv4RoutingTableEntry (RoutingTableEntry::CreateHostRouteTo
(host, interface)), m_sequenceNo (0),
                  m_metric (0),
                  m_changed (false),
                  m_validity (eslr::INVALID),
                  m_routeType (eslr::SECONDARY),
                  m_routeTag (0)
{
  /*cstrctr*/
}

RoutingTableEntry::~RoutingTableEntry ()
{
  /*dstrctr*/
}


/* 
* Routing Table
* \brief this implementation is for route table management functions.
* route tables are defined as main and backup. 
* 
* There are three type of routes added to both tables.
* 1. Main route -> the route is in the main table
* 2. Primary route -> the agent of the main route in the backup table
* 3. Backup route -> the secondary/backup route for the main route
* 
* The main routing table maintains the topology table,
* Depending on the routing table (e.g., main or backup), update, delete and invalidate methods are getting deffer. 
* For both tables, one method of actions is implemented. For an example there are no two separate functions to 
* delete, add, invalidate, etc. Only one function is implemented and, at the calling time, users need to specify
* the table; main or backup. 
* 
* For an example,
* m_routing.PrintRoutingTable (stream, RoutingTable::MAIN)
*/

RoutingTable::RoutingTable()
{
  /*cnstrctur*/}

RoutingTable::~RoutingTable ()
{
  /*dstrcter*/}

void 
RoutingTable::AddRoute (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table)
{
  NS_LOG_DEBUG (this << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));
    
  if (table == eslr::MAIN)
  {
    // create new route and add to the main table
    // route will be invalidated after timeout time
    NS_LOG_DEBUG ("Added a new Route to Main Table " << routingTableEntry->GetDestNetwork () << "/" 
                              << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

    bool isAvailable = IsLocalRouteAvailable (routingTableEntry->GetDestNetwork (), 
                                              routingTableEntry->GetDestNetworkMask ());
    if (isAvailable)
      return;

    RoutingTableEntry* route1 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), 
                                                       routingTableEntry->GetDestNetworkMask (), 
                                                       routingTableEntry->GetGateway (), 
                                                       routingTableEntry->GetInterface ());
    route1->SetValidity (routingTableEntry->GetValidity ());
    route1->SetSequenceNo (routingTableEntry->GetSequenceNo ());
    route1->SetRouteType (eslr::PRIMARY);
    route1->SetMetric (routingTableEntry->GetMetric ());
    route1->SetRouteChanged (true); 

    invalidateParams  p;
    p.invalidateTime = invalidateTime;
    p.deleteTime = deleteTime;
    p.settlingTime = settlingTime;
    p.invalidateType = eslr::EXPIRE;
    p.table = eslr::MAIN;

    Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));
    EventId invalidateEvent = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, route1, p);
    m_mainRoutingTable.push_front (std::make_pair (route1, invalidateEvent));

    // Note: Due to the REF.1, this may cause a bug. 
    delete routingTableEntry;    
  }
  else if (table == eslr::BACKUP)
  {
    if (settlingTime.GetSeconds () != 0)
    {
      // if settling time != 0, route is added to the backup table
      // route will be added to the main routing table after the settling time 
      NS_LOG_DEBUG ("Added a new Route to Backup Table and schedule an event to move it to main table after settling time expires " << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

      RoutingTableEntry* route2 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), 
                                                         routingTableEntry->GetDestNetworkMask (), 
                                                         routingTableEntry->GetGateway (), 
                                                         routingTableEntry->GetInterface ());
      route2->SetValidity (routingTableEntry->GetValidity ());
      route2->SetSequenceNo (routingTableEntry->GetSequenceNo ());
      route2->SetRouteType (eslr::PRIMARY);
      route2->SetMetric (routingTableEntry->GetMetric ());
      route2->SetRouteChanged (true); 

      Time delay = settlingTime + Seconds (m_rng->GetValue (0.0, 5.0));
      EventId moveToMainEvent = Simulator::Schedule (delay, &RoutingTable::MoveToMain, this, 
                                                     route2, invalidateTime, deleteTime, settlingTime);
      m_backupRoutingTable.push_front (std::make_pair (route2, moveToMainEvent));
      
      delete routingTableEntry;
    }
    else if (settlingTime.GetSeconds () == 0)
    {
      // if settling time = 0, it means that the route is for the backup table
      // therefore, route will be added to the backup table and invalidate 
      NS_LOG_DEBUG ("Added a new Route to Backup Table and schedule an event to invalidate it after invalidate time expires" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

      RoutingTableEntry* route3 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), 
                                                         routingTableEntry->GetDestNetworkMask (), 
                                                         routingTableEntry->GetGateway (), 
                                                         routingTableEntry->GetInterface ());
      route3->SetValidity (routingTableEntry->GetValidity ());
      route3->SetSequenceNo (routingTableEntry->GetSequenceNo ());
      route3->SetRouteType (routingTableEntry->GetRouteType ());
      route3->SetMetric (routingTableEntry->GetMetric ());
      route3->SetRouteChanged (true); 

      invalidateParams  p;
      p.invalidateTime = invalidateTime;
      p.deleteTime = deleteTime;
      p.settlingTime = settlingTime;
      p.invalidateType = eslr::EXPIRE;
      p.table = eslr::BACKUP;
      
      EventId invalidateEvent;

      if (routingTableEntry->GetRouteType () == eslr::PRIMARY)
      {
        invalidateEvent = EventId ();
      }
      else
      {      
        Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));        
        invalidateEvent = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, route3, p);
      }
      m_backupRoutingTable.push_front (std::make_pair (route3, invalidateEvent));
      
      delete routingTableEntry;      
    }
  }
}

void
RoutingTable::MoveToMain (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settlingTime)
{
  // This method will be triggered at the settling event expires
  // Route add and update methods will call this method
  // This method will then call the add or update route methods respectively.
  NS_LOG_FUNCTION (this << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

  RoutesI existingRoute;

  bool foundInMain = FindRouteRecord (routingTableEntry->GetDestNetwork (), 
                                      routingTableEntry->GetDestNetworkMask (), 
                                      routingTableEntry->GetGateway (), 
                                      existingRoute, 
                                      eslr::MAIN);

  if (foundInMain)
  {
    NS_LOG_DEBUG ("Main Table has a route," << *routingTableEntry << "update it.");

    UpdateNetworkRoute (routingTableEntry, invalidateTime, deleteTime, settlingTime, eslr::MAIN);
  }
  else
  {
    NS_LOG_DEBUG ("Main Table does not have a route, add the new route." << *routingTableEntry);

    // REF.1 : This may introduces some bugs.
    // However, at the movement, as both main and the primary routes are added together, 
    // this wont become a bug. 
    // In fact, if we add primary route first, wait settling time, and move it to the main,
    // due to the fact that the memory pointer will be deleted, 
    // this may cause a big. Need a fix
    AddRoute (routingTableEntry, invalidateTime, deleteTime, Seconds(0), eslr::MAIN);
  }
}

void 
RoutingTable::AddHostRoute (RoutingTableEntry *routingTableEntry, 
                            Time invalidateTime, 
                            Time deleteTime, 
                            Time settlingTime, 
                            eslr::Table table)
{
  // Host routes are added using this method
  NS_LOG_FUNCTION (this << *routingTableEntry);
  if (table == eslr::MAIN)
  {
    if ((invalidateTime.GetSeconds () == 0) &&
        (deleteTime.GetSeconds ()  == 0) &&
        (settlingTime.GetSeconds () == 0))        
    {
      // routes about the local interfaces are added using this part
      NS_LOG_DEBUG ("Added a new Host Route to Main Table (without expiration)" << 
                                           routingTableEntry->GetDestNetwork () << "/" << 
                                           int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

      RoutingTableEntry* route1 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), 
                                                         routingTableEntry->GetDestNetworkMask (), 
                                                         routingTableEntry->GetGateway (), 
                                                         routingTableEntry->GetInterface ());
      route1->SetValidity (routingTableEntry->GetValidity ());
      route1->SetSequenceNo (routingTableEntry->GetSequenceNo ());
      route1->SetRouteType (eslr::PRIMARY);
      route1->SetMetric (routingTableEntry->GetMetric ());
      if (routingTableEntry->GetDestNetwork () == "127.0.0.1")
      {
        route1->SetRouteChanged (false); 
      }
      else
      {
        route1->SetRouteChanged (true);       
      }     

      m_mainRoutingTable.push_back (std::make_pair (route1,EventId ()));

      delete routingTableEntry;      
    }
    else
    {
      // This part is added to handle host route records which are receiving form the netibors
      NS_LOG_DEBUG ("Added a new Host Route to Main Table (with expiration)" << 
                                        routingTableEntry->GetDestNetwork () << "/" << 
                                        int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

      RoutingTableEntry* route2 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), 
                                                         routingTableEntry->GetDestNetworkMask (), 
                                                         routingTableEntry->GetGateway (), 
                                                         routingTableEntry->GetInterface ());
      route2->SetValidity (routingTableEntry->GetValidity ());
      route2->SetSequenceNo (routingTableEntry->GetSequenceNo ());
      route2->SetRouteType (eslr::PRIMARY);
      route2->SetMetric (routingTableEntry->GetMetric ());
      route2->SetRouteChanged (true); 

      invalidateParams  p;
      p.invalidateTime = invalidateTime;
      p.deleteTime = deleteTime;
      p.settlingTime = settlingTime;
      p.invalidateType = eslr::EXPIRE;
      p.table = eslr::MAIN;

      Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));
      EventId invalidateEvent = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, route2, p);

      m_mainRoutingTable.push_back (std::make_pair (routingTableEntry,invalidateEvent));
      
      delete routingTableEntry;      
    }
  }
  else if (table == eslr::BACKUP)
  {
    if (settlingTime.GetSeconds () != 0)
    {
      // Add a route to the backup table as the primary route
      NS_LOG_DEBUG ("Added a new Host Route to Backup Table and schedule " <<
                    "an event to move it to main table after settling time expires" 
                    << routingTableEntry->GetDestNetwork () << 
                    "/" <<
                    int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

      RoutingTableEntry* route4 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), 
                                                         routingTableEntry->GetDestNetworkMask (), 
                                                         routingTableEntry->GetGateway (), 
                                                         routingTableEntry->GetInterface ());
      route4->SetValidity (routingTableEntry->GetValidity ());
      route4->SetSequenceNo (routingTableEntry->GetSequenceNo ());
      route4->SetRouteType (eslr::PRIMARY);
      route4->SetMetric (routingTableEntry->GetMetric ());
      route4->SetRouteChanged (true); 

      Time delay = settlingTime + Seconds (m_rng->GetValue (0.0, 5.0));
      EventId moveToMainEvent = Simulator::Schedule (delay, &RoutingTable::MoveToMain, 
                                                     this, route4, invalidateTime, 
                                                     deleteTime, settlingTime);

      m_backupRoutingTable.push_front (std::make_pair (routingTableEntry,moveToMainEvent));
      
      delete routingTableEntry;      
    }
    else if (settlingTime.GetSeconds () == 0)
    {
      // Add a route to the backup table as the secondary route
      NS_LOG_DEBUG ("Added a new Host Route to Backup" << 
                    " Table and schedule an event to invalidate it" <<
                    routingTableEntry->GetDestNetwork () << 
                    "/" <<
                    int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

      RoutingTableEntry* route3 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), 
                                                         routingTableEntry->GetDestNetworkMask (), 
                                                         routingTableEntry->GetGateway (), 
                                                         routingTableEntry->GetInterface ());
      route3->SetValidity (routingTableEntry->GetValidity ());
      route3->SetSequenceNo (routingTableEntry->GetSequenceNo ());
      route3->SetRouteType (eslr::SECONDARY);
      route3->SetMetric (routingTableEntry->GetMetric ());
      route3->SetRouteChanged (true); 

      invalidateParams  p;
      p.invalidateTime = invalidateTime;
      p.deleteTime = deleteTime;
      p.settlingTime = settlingTime;
      p.invalidateType = eslr::EXPIRE;
      p.table = eslr::BACKUP;

      Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));
      EventId invalidateEvent = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, route3, p);

      m_backupRoutingTable.push_front (std::make_pair (routingTableEntry,invalidateEvent));
      
      delete routingTableEntry;
    }
  }
}

bool 
RoutingTable::DeleteRoute (RoutingTableEntry *routingTableEntry, eslr::Table table)
{
  NS_LOG_FUNCTION (this << *routingTableEntry);
  bool retVal = false;

  if (table == eslr::MAIN)
  {
    NS_LOG_DEBUG ("Delete the Main Route");
    for (RoutesI it = m_mainRoutingTable.begin (); it != m_mainRoutingTable.end (); it++)
    {
      if (it->first == routingTableEntry)
      {
        //delete routingTableEntry;      
        m_mainRoutingTable.erase (it);
        retVal = true;
        break;
      }
    }
  }
  else if (table == eslr::BACKUP)
  { 
    NS_LOG_DEBUG ("Delete either Primary or Backup Route");
    for (RoutesI it = m_backupRoutingTable.begin (); it != m_backupRoutingTable.end (); it++)
    {
      if (it->first == routingTableEntry)            
      {
        delete routingTableEntry;      
        m_backupRoutingTable.erase (it);
        retVal = true;
        break;
      }
    }
  }
  return retVal;
}

bool 
RoutingTable::InvalidateRoute (RoutingTableEntry *routingTableEntry, invalidateParams param)
{
  NS_LOG_FUNCTION (this << *routingTableEntry);

  invalidateParams  p;
  p.invalidateTime = param.invalidateTime;
  p.deleteTime = param.deleteTime;
  p.settlingTime = param.settlingTime;
  p.invalidateType = eslr::EXPIRE;
  p.table = eslr::BACKUP;
  
  Time delay;
  bool retVal = false; 

	if (param.table == eslr::MAIN)
	{
    // Main Route (Which is in the Main Table)    
    bool foundInMain, foundPrimaryRoute, foundBackupRoute;
    RoutesI mainRoute =  FindGivenRouteRecord (routingTableEntry, foundInMain, eslr::MAIN);

    // Primary route (Main route's agent in the Backup Table)
    RoutesI primaryRoute = FindRouteInBackupForDestination (routingTableEntry->GetDestNetwork (), 
                                                routingTableEntry->GetDestNetworkMask (), 
                                                foundPrimaryRoute, eslr::PRIMARY);

    // Backup Route (the secondary route for the main route, which is in the Backup Table)
    RoutesI secondaryRoute = FindRouteInBackupForDestination (routingTableEntry->GetDestNetwork (), 
                                               routingTableEntry->GetDestNetworkMask (), 
                                               foundBackupRoute, eslr::SECONDARY);
                                               
		if (!foundInMain)
		{
		  // All routes to be invalidated has to be present in the table.
			NS_ABORT_MSG ("ESLR::InvalidateRoute - cannot find the route to update " << 
			               routingTableEntry->GetDestNetwork ());
		}
		
		if (param.invalidateType == eslr::EXPIRE)
		{
      if (foundBackupRoute && 
          (secondaryRoute->first->GetMetric () < primaryRoute->first->GetMetric ()) && 
          (Simulator::GetDelayLeft (secondaryRoute->second) > ((param.invalidateTime/3)*2)))
			{
			  // if there is a backup route, which is stable and the cost is better,
			  // than that of the primary route
			  // update both main and primary according to the backup route
			  // delete the backup route and let another backup route to come

			  NS_LOG_DEBUG ("Main route is expired. Update both main route " << *mainRoute->first << 
			                "and primary route " << *primaryRoute->first << 
			                " based on the secondary route.");
			  			  
        Ipv4Address destination = secondaryRoute->first->GetDestNetwork ();
        Ipv4Mask mask = secondaryRoute->first->GetDestNetworkMask ();
        Ipv4Address gateway = secondaryRoute->first->GetGateway ();
        uint32_t interface = secondaryRoute->first->GetInterface ();
        uint32_t cost = secondaryRoute->first->GetMetric ();
        uint16_t sequenceNo = secondaryRoute->first->GetSequenceNo ();
        
        secondaryRoute->first->SetValidity (eslr::INVALID);
        secondaryRoute->second.Cancel ();         
        m_backupRoutingTable.erase (secondaryRoute);        
        
        RoutingTableEntry* m_route = new RoutingTableEntry (destination, mask, gateway, interface);
        m_route->SetValidity (eslr::VALID);
        m_route->SetSequenceNo (sequenceNo);
        m_route->SetRouteType (eslr::PRIMARY);
        m_route->SetMetric (cost);
        m_route->SetRouteChanged (true);          
        
        // check this part        
        delete routingTableEntry;
        //delete mainRoute->first;
        mainRoute->first = m_route;
        
			  p.settlingTime = Seconds (0);
			  p.invalidateType = eslr::EXPIRE;
        p.table = eslr::MAIN;

        mainRoute->second.Cancel ();          
        Time delay = param.invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));
        mainRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, 
                                                 this, m_route, p); // pls chk
                
        RoutingTableEntry* p_route = new RoutingTableEntry (destination, mask, gateway, interface);
        p_route->SetValidity (eslr::VALID);
        p_route->SetSequenceNo (sequenceNo);
        p_route->SetRouteType (eslr::PRIMARY);
        p_route->SetMetric (cost);
        p_route->SetRouteChanged (true);           
        
        delete primaryRoute->first;
        primaryRoute->first = p_route;
        
        primaryRoute->second.Cancel ();         
        primaryRoute->second = EventId ();
				
				return retVal = true;			  			  
			}
			else
			{
			  // As a primary route is there (always) 
			  //the main route updates according to the primary route
			  NS_LOG_DEBUG ("Update the main route " << *mainRoute->first << "based on the primary route.");

        RoutingTableEntry* m_route = new RoutingTableEntry (primaryRoute->first->GetDestNetwork (), 
                                                            primaryRoute->first->GetDestNetworkMask (),
                                                            primaryRoute->first->GetGateway (), 
                                                            primaryRoute->first->GetInterface ());
        m_route->SetMetric (primaryRoute->first->GetMetric ());
        m_route->SetSequenceNo (primaryRoute->first->GetSequenceNo ());
        m_route->SetValidity (eslr::VALID);
        m_route->SetRouteType (eslr::PRIMARY);
        m_route->SetRouteChanged (true);

        // check this part        
        delete routingTableEntry;
        //delete mainRoute->first;
        mainRoute->first = m_route;

			  p.settlingTime = Seconds (0);
			  p.invalidateType = eslr::EXPIRE;
        p.table = eslr::MAIN;
        
        mainRoute->second.Cancel ();
        delay = param.invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));
        mainRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, 
                                                 m_route, p); // pls chk

        primaryRoute->second.Cancel ();                                                   
        primaryRoute->second = EventId ();
        
        return retVal = true;			  
			}      
		}
		else if (param.invalidateType == eslr::BROKEN_NEIGHBOR || param.invalidateType == eslr::BROKEN_INTERFACE)
		{ 
			if (foundBackupRoute)
			{
			  // if a backup route found, 
			  // as there is no other option,
			  // without considering the cost of the backup route, 
			  // update both main and primary routes based on the backup route.
			  NS_LOG_DEBUG ("The neighbor or the local interface is disconnected. Update both main route " <<
			                 *mainRoute->first << 
			                "and primary route " << *primaryRoute->first << 
			                " based on the secondary route.");			  
        
        Ipv4Address destination = secondaryRoute->first->GetDestNetwork ();
        Ipv4Mask mask = secondaryRoute->first->GetDestNetworkMask ();
        Ipv4Address gateway = secondaryRoute->first->GetGateway ();
        uint32_t interface = secondaryRoute->first->GetInterface ();
        uint32_t cost = secondaryRoute->first->GetMetric ();
        uint16_t sequenceNo = secondaryRoute->first->GetSequenceNo ();
        
        secondaryRoute->first->SetValidity (eslr::INVALID);          
        secondaryRoute->second.Cancel ();      
        m_backupRoutingTable.erase (secondaryRoute);                         
        
        RoutingTableEntry* m_route = new RoutingTableEntry (destination, mask, gateway, interface);
        m_route->SetValidity (eslr::VALID);
        m_route->SetSequenceNo (sequenceNo);
        m_route->SetRouteType (eslr::PRIMARY);
        m_route->SetMetric (cost);
        m_route->SetRouteChanged (true);
                  
        // check this part        
        delete routingTableEntry;
        //delete mainRoute->first;
        mainRoute->first = m_route;
        
			  p.settlingTime = Seconds (0);
			  p.invalidateType = eslr::EXPIRE;
        p.table = eslr::MAIN;

        mainRoute->second.Cancel ();
        
        Time delay = param.invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));
        mainRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, 
                                                 this, m_route, p); // pls chk
                
        RoutingTableEntry* p_route = new RoutingTableEntry (destination, mask, gateway, interface);
        p_route->SetValidity (eslr::VALID);
        p_route->SetSequenceNo (sequenceNo);
        p_route->SetRouteType (eslr::PRIMARY);
        p_route->SetMetric (cost);
        p_route->SetRouteChanged (true);          
        
        delete primaryRoute->first;
        primaryRoute->first = p_route;        
        
        primaryRoute->second.Cancel ();         
        primaryRoute->second = EventId (); 
        
        std::cout << "updating for invalid neighbor or interface" << std::endl;         
				
				return retVal = true;
			}
			else
			{
			  // As there is no backup route found
			  // delete both main and associated primary route
        NS_LOG_DEBUG ("The neighbor or the local interface is disconnected. "<<
                      "No backup route is found. Deleting both main and primary routes!.");

        Time delay = param.deleteTime + Seconds (m_rng->GetValue (0.0, 5.0));
        
        if (foundInMain)
        { 
          mainRoute->first->SetValidity (eslr::DISCONNECTED);
          mainRoute->first->SetRouteChanged (true);
          
          mainRoute->second.Cancel ();
          mainRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, 
                                                   routingTableEntry, eslr::MAIN); // pls chk

          // do not uncomment this
					//delete routingTableEntry;
        }
        if (foundPrimaryRoute)
        { 
          primaryRoute->second.Cancel ();

          primaryRoute->first->SetValidity (eslr::DISCONNECTED);
          primaryRoute->first->SetRouteChanged (true);
          primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, 
                                                      primaryRoute->first, eslr::BACKUP);
        }        
        return (retVal = true); 
			}
		}		
		else if (param.invalidateType == eslr::BROKEN)
		{
		  // the main route is marked as broken route
		  // There is no need of requesting backup route.
		  // As the route is about a broken destination network, backup route about the same destination is also
		  // broken. As there is no point of retrieving the backup route, in this implementation, simply,
		  // both Main and primary routes are marked as disconnected and deleted.

      NS_LOG_DEBUG ("Routes are broken and no backup routes found. Deleting both main and primary routes!.");

      Time delay = param.deleteTime + Seconds (m_rng->GetValue (0.0, 5.0));
      
      if (foundInMain)
      { 
        mainRoute->first->SetValidity (eslr::DISCONNECTED);
        mainRoute->first->SetRouteChanged (true);
        
        mainRoute->second.Cancel ();
        mainRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, 
                                                 routingTableEntry, eslr::MAIN); // pls chk

        // do not uncomment this
				//delete routingTableEntry;
      }
      if (foundPrimaryRoute)
      { 
        primaryRoute->second.Cancel ();

        primaryRoute->first->SetValidity (eslr::DISCONNECTED);
        primaryRoute->first->SetRouteChanged (true);
        primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, 
                                                    primaryRoute->first, eslr::BACKUP);
      }        
      return (retVal = true); 
		}
	}
	else if (param.table == eslr::BACKUP)
	{
    bool foundBackupRoute;
    RoutesI secondaryRoute =  FindGivenRouteRecord (routingTableEntry, foundBackupRoute, eslr::BACKUP);
                                               
    if (!foundBackupRoute)
    {
			NS_ABORT_MSG ("ESLR::InvalidateRoute - cannot find the route to update");		  
    }
    
		if (routingTableEntry->GetRouteType () == eslr::SECONDARY)
		{
		  secondaryRoute->first->SetValidity (eslr::INVALID);
      secondaryRoute->second.Cancel (); 
      delete routingTableEntry;
      m_backupRoutingTable.erase (secondaryRoute); 
		}
		else if (routingTableEntry->GetRouteType () == eslr::PRIMARY)
		{	
      // In ESLR, 
      // primary routes are never invalidated. 
      // such routes are invalidated along with the main routes.
      // even accidentally they do, in this point, that will be corrected.

      secondaryRoute->second.Cancel (); 
      secondaryRoute->second = EventId ();  
      
      return (retVal = true);
		}
		else
		{
		  return (retVal = false);
		}
	}

  return retVal;
}

bool 
RoutingTable::UpdateNetworkRoute (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table)
{
  NS_LOG_FUNCTION (this << *routingTableEntry);
  
  bool retVal = false;

  if (table == eslr::MAIN)
  {
    // Main Route (Which is in the Main Table)    
    bool foundInMain, foundPrimaryRoute, foundBackupRoute;
    RoutesI mainRoute =  FindValidRouteRecordForDestination (routingTableEntry->GetDestNetwork (), 
                                                 routingTableEntry->GetDestNetworkMask (), 
                                                 routingTableEntry->GetGateway (), foundInMain, eslr::MAIN);

    // Primary route (Main route's agent in the Backup Table)
    RoutesI primaryRoute =  FindGivenRouteRecord (routingTableEntry, foundPrimaryRoute, eslr::BACKUP);

    // Backup Route (the secondary route for the main route, which is in the Backup Table)
    RoutesI secondaryRoute = FindRouteInBackupForDestination (routingTableEntry->GetDestNetwork (), 
                                                  routingTableEntry->GetDestNetworkMask (), 
                                                  foundBackupRoute, eslr::SECONDARY);    
    // For the routes learned by neighbors, 
    // update both Main and Primary route in Main and backup routing table respectively.

    if (foundInMain && 
       (routingTableEntry->GetGateway () != Ipv4Address::GetZero ()) && 
       (mainRoute->first->GetValidity () != eslr::DISCONNECTED))
    {
      NS_LOG_DEBUG ("Update the Main Route " << *mainRoute->first << " and the Primary Route"); 
      
      // Find any backup route that has lower cost than the primary route
      // Make sure that the main route has the lowest cost available.
      if (foundBackupRoute && 
         (secondaryRoute->first->GetMetric () < routingTableEntry->GetMetric ()))
      {      
        Ipv4Address destination = secondaryRoute->first->GetDestNetwork ();
        Ipv4Mask mask = secondaryRoute->first->GetDestNetworkMask ();
        Ipv4Address gateway = secondaryRoute->first->GetGateway ();
        uint32_t interface = secondaryRoute->first->GetInterface ();
        uint32_t cost = secondaryRoute->first->GetMetric ();
        uint16_t sequenceNo = secondaryRoute->first->GetSequenceNo ();
        
        secondaryRoute->first->SetValidity (eslr::INVALID);        
        secondaryRoute->second.Cancel ();
        m_backupRoutingTable.erase (secondaryRoute);                         
        
        RoutingTableEntry* m_route = new RoutingTableEntry (destination, mask, gateway, interface);
        m_route->SetValidity (eslr::VALID);
        m_route->SetSequenceNo (sequenceNo);
        m_route->SetRouteType (eslr::PRIMARY);
        m_route->SetMetric (cost);
        m_route->SetRouteChanged (true);  
              
		    delete mainRoute->first;
        mainRoute->first = m_route;
        
        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::EXPIRE;
        p.table = eslr::MAIN;

        mainRoute->second.Cancel ();        
        Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));
        mainRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, 
                                                 this, m_route, p); // pls chk
                
        RoutingTableEntry* p_route = new RoutingTableEntry (destination, mask, gateway, interface);
        p_route->SetValidity (eslr::VALID);
        p_route->SetSequenceNo (sequenceNo);
        p_route->SetRouteType (eslr::PRIMARY);
        p_route->SetMetric (cost);
        p_route->SetRouteChanged (true);            
        
        delete routingTableEntry; // pls chk
	      //delete primaryRoute->first;
        primaryRoute->first = p_route;
        
        primaryRoute->second.Cancel ();         
        primaryRoute->second = EventId ();
        
        return retVal = true;
      }
      else
      {      		
        RoutingTableEntry* m_route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (),
                                                            routingTableEntry->GetDestNetworkMask (),
                                                            routingTableEntry->GetGateway (), 
                                                            routingTableEntry->GetInterface ());
        m_route->SetValidity (eslr::VALID);
        m_route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        m_route->SetRouteType (eslr::PRIMARY);
        m_route->SetMetric (routingTableEntry->GetMetric ());
        m_route->SetRouteChanged (true);

        delete mainRoute->first;
        //delete routingTableEntry;
        mainRoute->first = m_route;

        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::EXPIRE;
        p.table = eslr::MAIN;
        
        mainRoute->second.Cancel ();
        Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));
        mainRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, 
                                                 this, m_route, p); 
        
        primaryRoute->second.Cancel ();
        primaryRoute->second = EventId (); 
        return retVal = true;                
      }
    }
  }
  else if (table == eslr::BACKUP)
  {    
    bool foundPrimary, foundSecondary;

                                        
    // Primary route (Main route's agent in the Backup Table)
    RoutesI primaryRoute = FindRouteInBackupForDestination (routingTableEntry->GetDestNetwork (), 
                                                routingTableEntry->GetDestNetworkMask (), 
                                                foundPrimary, eslr::PRIMARY);

    // Backup Route (the secondary route for the main route, which is in the Backup Table)
    RoutesI secondaryRoute = FindRouteInBackupForDestination (routingTableEntry->GetDestNetwork (), 
                                               routingTableEntry->GetDestNetworkMask (), 
                                               foundSecondary, eslr::SECONDARY);

    if (routingTableEntry->GetRouteType () == eslr::PRIMARY)
    {        
      if (foundPrimary && 
         (primaryRoute->first->GetValidity () != eslr::DISCONNECTED))
      {	      
        NS_LOG_DEBUG ("Update the primary route.");      
        // Update the primary route and,
        // schedule and event to add the updated route after a settling time.
        RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), 
                                                          routingTableEntry->GetDestNetworkMask (),
                                                          routingTableEntry->GetGateway (), 
                                                          routingTableEntry->GetInterface ());

        route->SetValidity (routingTableEntry->GetValidity ());
        route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        route->SetRouteType (eslr::PRIMARY);
        route->SetMetric (routingTableEntry->GetMetric ());
        route->SetRouteChanged (true);
      
        delete primaryRoute->first;
        primaryRoute->first = route;           

        if (routingTableEntry->GetMetric () == primaryRoute->first->GetMetric ())
        {
          // As the cost is same, the event will not cancel off.
          // let the event to be expired and move/update the route to main table.
          Time delay = Simulator::GetDelayLeft (primaryRoute->second) + Seconds (m_rng->GetValue (0.0, 4.0));
          
          primaryRoute->second.Cancel ();         
					primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::MoveToMain, 
                                                      this, route, invalidateTime, 
                                                      deleteTime, settlingTime); 
        }
        else
        { 
          Time delay = settlingTime + Seconds (m_rng->GetValue (0.0, 5.0)); 
					
					primaryRoute->second.Cancel ();					
          primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::MoveToMain, 
                                                      this, route, invalidateTime, 
                                                      deleteTime, settlingTime);          
        }

        delete routingTableEntry;
        return retVal = true; 
      }
      else
        return retVal = false;
    }
    else if (routingTableEntry->GetRouteType () == eslr::SECONDARY) 
    {
      if (foundSecondary && 
         (secondaryRoute->first->GetValidity () != eslr::DISCONNECTED))
      {        
        // update the secondary record and,
        // schedule and event to expire the route.
        RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), 
                                                          routingTableEntry->GetDestNetworkMask (), 
                                                          routingTableEntry->GetGateway (), 
                                                          routingTableEntry->GetInterface ());
        route->SetValidity (routingTableEntry->GetValidity ());
        route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        route->SetRouteType (eslr::SECONDARY);
        route->SetMetric (routingTableEntry->GetMetric ());
        route->SetRouteChanged (true);

        delete secondaryRoute->first;
        secondaryRoute->first = route;

        secondaryRoute->second.Cancel (); 

        NS_LOG_DEBUG ("Update the backup route.");
        
        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::EXPIRE;
        p.table = eslr::BACKUP;      

        Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 2.0));                
        secondaryRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, 
                                                      this, route, p);   
                                                      
        delete routingTableEntry;                                                       
        return retVal = true;            
      }
      else 
      {
        return retVal = false;
      }
    }
  }
  return retVal;
}  

void 
RoutingTable::UpdateLocalRoute (Ipv4Address destination, Ipv4Mask netMask, uint32_t metric)
{
  for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
  {
    if ((it->first->GetDestNetwork () == destination) &&
        (it->first->GetDestNetworkMask () == netMask) &&
        (it->first->GetGateway () == Ipv4Address::GetZero ()))
    {
      //std::cout << " updating" << int (metric) << std::endl;
      it->first->SetMetric (metric);
      it->first->SetRouteChanged (true);
      return;
    }
  }
}

void 
RoutingTable::InvalidateRoutesForGateway (Ipv4Address gateway,  Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table)
{
  NS_LOG_FUNCTION (this << gateway); 

  invalidateParams  p;
  p.invalidateTime = invalidateTime;
  p.deleteTime = deleteTime;
  p.settlingTime = settlingTime;
  p.invalidateType = eslr::BROKEN;
  p.table = table; 
  
  if (table == eslr::MAIN)
  {
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      if ((it->first->GetGateway () == gateway) && 
          (it->first->GetValidity () == eslr::VALID))
      {
        p.invalidateType = eslr::BROKEN_NEIGHBOR;
        it->second.Cancel ();
        it->second  = Simulator::Schedule (MicroSeconds (m_rng->GetValue (0.0, 2.0)),
                                           &RoutingTable::InvalidateRoute, 
                                           this, 
                                           it->first, 
                                           p);
        //InvalidateRoute (it->first, p);
      }
    }  
  }
  if (table == eslr::BACKUP)
  {
    for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {
      if ((it->first->GetGateway () == gateway) && 
          (it->first->GetValidity () == eslr::VALID) &&
          (it->first->GetRouteType () == eslr::SECONDARY))
      {
        p.invalidateType = eslr::BROKEN;      
        it->second.Cancel ();
        it->second  = Simulator::Schedule (MicroSeconds (m_rng->GetValue (0.0, 2.0)),
                                           &RoutingTable::InvalidateRoute,
                                           this, 
                                           it->first, 
                                           p);
        //InvalidateRoute (it->first, p);
      }
    }  
  }    
}

void 
RoutingTable::InvalidateRoutesForInterface (uint32_t interface,  Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table)
{
  NS_LOG_FUNCTION (this << interface);

  invalidateParams  p;
  p.invalidateTime = invalidateTime;
  p.deleteTime = deleteTime;
  p.settlingTime = settlingTime;
  p.invalidateType = eslr::BROKEN;
  p.table = table; 
  
  if (table == eslr::MAIN)
  {
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      if ((it->first->GetInterface () == interface) && 
          (it->first->GetValidity () == eslr::VALID))
      {
        p.invalidateType = eslr::BROKEN_INTERFACE;      
        it->second.Cancel ();
        it->second  = Simulator::Schedule (MicroSeconds (m_rng->GetValue (0.0, 2.0)),
                                           &RoutingTable::InvalidateRoute,
                                           this, 
                                           it->first, 
                                           p);
        //InvalidateRoute (it->first, p);
      }
    }  
  }
  if (table == eslr::BACKUP)
  {
    for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {
      if ((it->first->GetInterface () == interface) && 
          (it->first->GetValidity () == eslr::VALID) &&
          (it->first->GetRouteType () == eslr::SECONDARY))
      {
        p.invalidateType = eslr::BROKEN;      
        it->second.Cancel ();
        it->second  = Simulator::Schedule (MicroSeconds (m_rng->GetValue (0.0, 2.0)),
                                           &RoutingTable::InvalidateRoute,
                                           this, 
                                           it->first, 
                                           p);
        //InvalidateRoute (it->first, p);
      }
    }  
  }  
}

bool 
RoutingTable::InvalidateBrokenRoute (Ipv4Address destAddress, Ipv4Mask destMask, Ipv4Address gateway, Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table)
{
  bool retVal = false;

  invalidateParams  p;
  p.invalidateTime = invalidateTime;
  p.deleteTime = deleteTime;
  p.settlingTime = settlingTime;
  p.invalidateType = eslr::BROKEN;
  p.table = table;     

  if (table == eslr::MAIN)
  {
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destAddress) &&
          (it->first->GetDestNetworkMask () == destMask) &&
          (it->first->GetGateway () == gateway) && 
          (it->first->GetValidity () == eslr::VALID))
      {
        it->second.Cancel ();
        it->second  = Simulator::Schedule (MicroSeconds (m_rng->GetValue (0.0, 2.0)), 
                                           &RoutingTable::InvalidateRoute, 
                                           this, 
                                           it->first, 
                                           p);
        retVal = true;
      }
    }  
  }
  if (table == eslr::BACKUP)
  {
    for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destAddress) &&
          (it->first->GetDestNetworkMask () == destMask) &&
          (it->first->GetGateway () == gateway) && 
          (it->first->GetValidity () == eslr::VALID) &&
          (it->first->GetRouteType () == eslr::SECONDARY))
      {
        it->second.Cancel ();
        it->second  = Simulator::Schedule (MicroSeconds (m_rng->GetValue (0.0, 2.0)), 
                                           &RoutingTable::InvalidateRoute, 
                                           this, 
                                           it->first, 
                                           p);
        retVal = true;
      }
    }  
  }   
  
  return retVal;
}

bool 
RoutingTable::FindRouteRecord (Ipv4Address destination, Ipv4Mask netMask, Ipv4Address gateway, RoutesI &retRoutingTableEntry, eslr::Table table)
{
  bool retVal = false;

  if (table == eslr::MAIN)
  {
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destination) &&
          (it->first->GetDestNetworkMask () == netMask) &&
          (it->first->GetGateway () == gateway))
      {
        retRoutingTableEntry = it;
        retVal = true;
        break;
      }
    }
  }
  else if (table == eslr::BACKUP)
  {
    for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destination) &&
          (it->first->GetDestNetworkMask () == netMask) &&
          (it->first->GetGateway () == gateway))
      {
        retRoutingTableEntry = it;
        retVal = true;
        break;
      }
    } 
  }
  return retVal;
}

bool 
RoutingTable::FindRouteRecord (Ipv4Address destination, Ipv4Mask netMask, RoutesI &retRoutingTableEntry, eslr::Table table)
{
  bool retVal = false;

  if (table == eslr::MAIN)
  {
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destination) &&
          (it->first->GetDestNetworkMask () == netMask) &&
          (it->first->GetGateway () != Ipv4Address::GetZero ()))
      {
        retRoutingTableEntry = it;
        retVal = true;
        break;
      }
    }
  }
  else if (table == eslr::BACKUP)
  {
    for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destination) &&
          (it->first->GetDestNetworkMask () == netMask))
      {
        retRoutingTableEntry = it;
        retVal = true;
        break;
      }
    } 
  }
  return retVal;
}

bool 
RoutingTable::FindValidRouteRecord (Ipv4Address destination, Ipv4Mask netMask, RoutesI &retRoutingTableEntry, eslr::Table table)
{
  bool retVal = false;

  if (table == eslr::MAIN)
  {
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destination) &&
          (it->first->GetDestNetworkMask () == netMask) &&
          (it->first->GetGateway () != Ipv4Address::GetZero ()) &&
          (it->first->GetValidity () == eslr::VALID))
      {
        retRoutingTableEntry = it;
        retVal = true;
        break;
      }
    }
  }
  else if (table == eslr::BACKUP)
  {
    for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destination) &&
          (it->first->GetDestNetworkMask () == netMask) &&
          (it->first->GetValidity () == eslr::VALID))
      {
        retRoutingTableEntry = it;
        retVal = true;
        break;
      }
    } 
  }
  return retVal;
}

RoutingTable::RoutesI
RoutingTable::FindGivenRouteRecord (RoutingTableEntry *route, bool &found, eslr::Table table)
{
  RoutesI foundRoute;
  bool retVal = false;
  if (table == eslr::MAIN)
  {
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      if (it->first == route)
      {
        retVal = true;
        foundRoute = it;
      }
    }
  }
  else if (table == eslr::BACKUP)
  {
    for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {
      if (it->first == route)
      {
        retVal = true;
        foundRoute = it;
      }      
    } 
  }
  found = retVal;
  return foundRoute;
}

RoutingTable::RoutesI
RoutingTable::FindValidRouteRecordForDestination (Ipv4Address destination, Ipv4Mask netMask, Ipv4Address gateway, bool &found, eslr::Table table)
{
  RoutesI foundRoute;
  bool retVal = false;
  if (table == eslr::MAIN)
  {
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destination) &&
          (it->first->GetDestNetworkMask () == netMask) &&
          (it->first->GetGateway () == gateway) &&
          (it->first->GetValidity () == eslr::VALID))
      {
        retVal = true;
        foundRoute = it;
      }
    }
  }
  else if (table == eslr::BACKUP)
  {
    for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {
      if ((it->first->GetDestNetwork () == destination) &&
          (it->first->GetDestNetworkMask () == netMask) &&
          (it->first->GetGateway () == gateway) &&
          (it->first->GetValidity () == eslr::VALID))
      {
        retVal = true;
        foundRoute = it;
      }      
    } 
  }
  found = retVal;
  return foundRoute;
}

bool 
RoutingTable::FindRouteInBackup (Ipv4Address destination, Ipv4Mask netMask, RoutesI &retRoutingTableEntry, eslr::RouteType routeType)
{
  bool retVal = false;

  for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
  {
    if ((it->first->GetDestNetwork () == destination) &&
        (it->first->GetDestNetworkMask () == netMask) &&
        (it->first->GetValidity () == eslr::VALID) &&
        (it->first->GetRouteType () == routeType))
    {
      retRoutingTableEntry = it;
      retVal = true;
    }
  }
  return retVal;
}

RoutingTable::RoutesI
RoutingTable::FindRouteInBackupForDestination (Ipv4Address destination, Ipv4Mask netMask, bool &found, eslr::RouteType routeType)
{
  bool retVal = false;
  RoutesI foundRoute;
  for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
  {
    if ((it->first->GetDestNetwork () == destination) &&
        (it->first->GetDestNetworkMask () == netMask) &&
        (it->first->GetValidity () == eslr::VALID) &&
        (it->first->GetRouteType () == routeType))
    {
      foundRoute = it;
      retVal = true;
    }
  }
  
  found = retVal;
  return foundRoute;
}

bool 
RoutingTable::IsLocalRouteAvailable (Ipv4Address destination, Ipv4Mask netMask)
{
  bool retVal = false;

  for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
  {
    if ((it->first->GetDestNetwork () == destination) &&
        (it->first->GetDestNetworkMask () == netMask) &&
        (it->first->GetGateway () == Ipv4Address::GetZero ()))
    {
      retVal = true;
    }
  }
  return retVal;
}

void 
RoutingTable::ReturnRoutingTable (RoutingTableInstance &instance, eslr::Table table)
{
  NS_LOG_FUNCTION (this);
  
  if (table == eslr::MAIN)
  {
    // Note: In the case of Main routing table, a newly created separate instance is returned. 
    // However, for proper memory, 
    // table instance has to be cleared at the place where requesting the main routing table.
    
    NS_LOG_DEBUG ("Create a fresh instance of the Main table and return");
    
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
        RoutingTableEntry* route;
      route  =  new RoutingTableEntry (it->first->GetDestNetwork (), it->first->GetDestNetworkMask (), it->first->GetGateway (), it->first->GetInterface ());    
      route->SetValidity (it->first->GetValidity ());
      route->SetSequenceNo (it->first->GetSequenceNo ());
      route->SetRouteType (it->first->GetRouteType ());
      route->SetMetric (it->first->GetMetric ());
      route->SetRouteChanged (it->first->GetRouteChanged ());
        
      instance.push_front(std::make_pair (route, EventId ()));
    }
  }
  else if (table == eslr::BACKUP)
  { 
    NS_LOG_DEBUG ("Return the existing instance of the Backup Table");      
    for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {    
      instance.push_front(std::make_pair (it->first, it->second));
    }
  }
  else // invalid routing table type
    NS_ABORT_MSG ("No specified routing table found. Aborting.");
}

void 
RoutingTable::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, eslr::Table table) const
{
  std::ostream* os = stream->GetStream ();

  if (table == eslr::MAIN)
  {
    *os << "Destination         Gateway          If  Seq#    Metric  Validity     Changed Expire in (s)" << '\n';
    *os << "------------------  ---------------  --  ------  ------  --------     ------- -------------" << '\n';

    for (RoutesCI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      RoutingTableEntry *route = it->first;
      //eslr::Validity validity = route->GetValidity ();

      //if (validity == eslr::VALID || validity == eslr::LHOST || validity == eslr::INVALID || validity == eslr::DISCONNECTED)
      //{
        std::ostringstream dest, gateway, val;
        
        dest << route->GetDestNetwork () << "/" << int (route->GetDestNetworkMask ().GetPrefixLength ());
        *os << std::setiosflags (std::ios::left) << std::setw (20) << dest.str ();
        
        gateway << route->GetGateway ();
        *os << std::setiosflags (std::ios::left) << std::setw (17) << gateway.str ();
        
        *os << std::setiosflags (std::ios::left) << std::setw (4) << route->GetInterface ();
        
        *os << std::setiosflags (std::ios::left) << std::setw (8) << route->GetSequenceNo ();
        
        *os << std::setiosflags (std::ios::left) << std::setw (8) << route->GetMetric ();
        
        if (route->GetValidity () == eslr::VALID)
          val << "VALID";
        else if (route->GetValidity () == eslr::INVALID)
          val << "INVALID";
        else if (route->GetValidity () == eslr::LHOST)
          val << "Loc. Host";
        else if (route->GetValidity () == eslr::DISCONNECTED)
          val << "Disconnected";
        else
          val << "garbage";            
        *os << std::setiosflags (std::ios::left) << std::setw (13) << val.str ();
        
        *os << std::setiosflags (std::ios::left) << std::setw (8) << route->GetRouteChanged ();
        
        *os << std::setiosflags (std::ios::left) << std::setw (10) << Simulator::GetDelayLeft (it->second).GetSeconds ();
       
        *os << '\n';
      //}        
    }
    //*os << "---------------------------------------------------------------------------------------" << '\n';
  }
  else if (table == eslr::BACKUP)
  {
    *os << "Destination         Gateway          If  Seq#    Metric  Validity      Pri/Sec Next Event (s)" << '\n';
    *os << "------------------  ---------------  --  ------  ------  ------------  ------- --------------" << '\n';

    for (RoutesCI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
    {

      RoutingTableEntry *route = it->first;

      std::ostringstream dest, gateway, validity, preSec;

      dest << route->GetDestNetwork () << "/" << int (route->GetDestNetworkMask ().GetPrefixLength ());
      *os << std::setiosflags (std::ios::left) << std::setw (20) << dest.str ();
      
      gateway << route->GetGateway ();
      *os << std::setiosflags (std::ios::left) << std::setw (17) << gateway.str ();
      
      *os << std::setiosflags (std::ios::left) << std::setw (4) << route->GetInterface ();
      
      *os << std::setiosflags (std::ios::left) << std::setw (8) << route->GetSequenceNo ();
      
      *os << std::setiosflags (std::ios::left) << std::setw (8) << route->GetMetric ();
      
      if (route->GetValidity () == eslr::VALID)
        validity << "VALID";
      else if (route->GetValidity () == eslr::INVALID)
          validity << "INVALID";
      else if (route->GetValidity () == eslr::DISCONNECTED)
          validity << "Disconnected";               
      *os << std::setiosflags (std::ios::left) << std::setw (14) << validity.str ();
      
      if (route->GetRouteType () == eslr::PRIMARY)
        preSec << "P";
      else
        preSec << "S";
      *os << std::setiosflags (std::ios::left) << std::setw (8) << preSec.str ();
      
      *os << std::setiosflags (std::ios::left) << std::setw (10) << Simulator::GetDelayLeft (it->second).GetSeconds ();
      *os << '\n';
    }
    //*os << "-----------------------------------------------------------------------------------------" << '\n';
  }
  else // invalid routing table type
    NS_ABORT_MSG ("No specified routing table found. Aborting.");
}

void 
RoutingTable::ToggleRouteChanged ()
{
  NS_LOG_FUNCTION (this);
  for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
  {
    if (it->first->GetValidity () == eslr::VALID)
    {
      it->first->SetRouteChanged (false);
    }
  }
}

void 
RoutingTable::IncrementSeqNo ()
{
  NS_LOG_FUNCTION (this);
  for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
  {
    if ((it->first->GetDestNetwork () != "127.0.0.1") && (it->first->GetGateway () == Ipv4Address::GetZero ()))
    {        
      it->first->SetSequenceNo (it->first->GetSequenceNo () + 2);
      it->first->SetRouteChanged (false);
    }
  }
}

void 
RoutingTable::AssignStream (int64_t stream)
{ 
  m_rng = CreateObject<UniformRandomVariable> ();
  m_rng->SetStream (stream);
}

bool 
RoutingTable::ReturnRoute (Ipv4Address destination, Ptr<NetDevice> dev, RoutesI &retRoutingTableEntry)
{
  NS_LOG_FUNCTION (this << destination);  

  Ipv4Address foundDestination;
  Ipv4Mask foundMask;
  
  bool retVal = false;

  NS_LOG_LOGIC ("Searching for a route to " << destination);

  for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
  {
    if (it->first->GetValidity () == eslr::VALID)
    {        
      foundDestination = it->first->GetDestNetwork ();
      foundMask = it->first->GetDestNetworkMask ();
      
      if (foundMask.IsMatch (destination, foundDestination))
      {
        NS_LOG_LOGIC ("found a route " << it->first << ", with the mask " << foundMask);
        
        if ((!dev) || (dev == m_ipv4->GetNetDevice (it->first->GetInterface ())))
        {
          retRoutingTableEntry = it;
          // As a route is found it is not necessary to iterate through the routing table anymore
          return retVal = true;
        }
      }
    }
  }
  return retVal;
}


std::ostream & operator << (std::ostream& os, const RoutingTableEntry& rte)
{
  os << static_cast<const Ipv4RoutingTableEntry &>(rte);
  os << ", metric: " << int (rte.GetMetric ()) << ", tag: " << int (rte.GetRouteTag ());

  return os;
}
}// end of eslr
}// end of ns3
