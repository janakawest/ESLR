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
 * route tables are defined as main and backup. There are three type of routes added to the both tables.
 * 1. Main route -> the route is in the main table
 * 2. Primary route -> the agent of the main route in the backup table
 * 3. Backup route -> the secondary/backup route for the main route
 * The main routing table maintains the topology table,
 * Depending on the routing table (e.g., main or backup), update, delete and invalidate methods are getting deffer. 
 * For both tables, one method of actions is implemented. For an example there are no two separate functions to 
 * delete, add, invalidate, etc. Only one function is implemented and, at the calling time, users need to specify
 * the table; main or backup. 
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
      NS_LOG_DEBUG ("Added a new Route to Main Table " << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

      bool isAvailable = IsLocalRouteAvailable (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask ());
      if (isAvailable)
        return;

      RoutingTableEntry* route1 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
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

      Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 10.0));
      EventId invalidateEvent = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, route1, p);
      m_mainRoutingTable.push_front (std::make_pair (route1, invalidateEvent));
    }
    else if (table == eslr::BACKUP)
    {
      if (settlingTime.GetSeconds () != 0)
      {
        // if settling time != 0, route is added to the backup table
        // route will be added to the main routing table after the settling time 
        NS_LOG_DEBUG ("Added a new Route to Backup Table and schedule an event to move it to main table after settling time expires " << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

        RoutingTableEntry* route2 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
        route2->SetValidity (routingTableEntry->GetValidity ());
        route2->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        route2->SetRouteType (eslr::PRIMARY);
        route2->SetMetric (routingTableEntry->GetMetric ());
        route2->SetRouteChanged (true); 

        Time delay = settlingTime + Seconds (m_rng->GetValue (0.0, 5.0));
        EventId moveToMainEvent = Simulator::Schedule (delay, &RoutingTable::MoveToMain, this, route2, invalidateTime, deleteTime, settlingTime);
        m_backupRoutingTable.push_front (std::make_pair (route2, moveToMainEvent));
      }
      else if (settlingTime.GetSeconds () == 0)
      {
        // if settling time = 0, it means that the route is for the backup table
        // therefore, route will be added to the backup table and invalidated after the timeout time
        NS_LOG_DEBUG ("Added a new Route to Backup Table and schedule an event to invalidate it after invalidate time expires" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

        RoutingTableEntry* route3 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
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
          //Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 20.0));
          invalidateEvent = EventId ();
        }
        else
        {
          Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 10.0));        
          invalidateEvent = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, route3, p);
        }
        m_backupRoutingTable.push_front (std::make_pair (route3, invalidateEvent));
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

    bool foundInMain = FindRouteRecord (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), existingRoute, eslr::MAIN);

    if (foundInMain)
    {
      NS_LOG_DEBUG ("Main Table has a route," << *routingTableEntry << "update it.");
			
			RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
      route->SetValidity (routingTableEntry->GetValidity ());
      route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
      route->SetRouteType (routingTableEntry->GetRouteType ());
      route->SetMetric (routingTableEntry->GetMetric ());
      route->SetRouteChanged (true); 

      UpdateNetworkRoute (route, invalidateTime, deleteTime, settlingTime, eslr::MAIN);
    }
    else
    {
      NS_LOG_DEBUG ("Main Table does not have a route, add the new route." << *routingTableEntry);
 
			RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
      route->SetValidity (routingTableEntry->GetValidity ());
      route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
      route->SetRouteType (routingTableEntry->GetRouteType ());
      route->SetMetric (routingTableEntry->GetMetric ());
      route->SetRouteChanged (true); 

      AddRoute (route, invalidateTime, deleteTime, Seconds(0), eslr::MAIN);
    } 

    // Mark the route as PRIMARY
    NS_LOG_DEBUG ("Now, set the relevant route in the Backup table as Primary Route." << *routingTableEntry);      

    for (RoutesI it = m_backupRoutingTable.begin (); it != m_backupRoutingTable.end (); it++)
    {
      if (it->first == routingTableEntry)
      {
        it->first->SetRouteType (eslr::PRIMARY);
        it->second.Cancel ();
        it->second = EventId (); // Do not invalidate until main table decides
      }
      else if ((it->first->GetDestNetwork () == routingTableEntry->GetDestNetwork ()) && 
               (it->first->GetDestNetworkMask () == routingTableEntry->GetDestNetworkMask ()) &&
               (it->first->GetGateway () != routingTableEntry->GetGateway ()))
      {
        it->first->SetRouteType (eslr::SECONDARY);
        it->second.Cancel ();

        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::EXPIRE;
        p.table = eslr::BACKUP;

        Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 10.0));
        it->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, it->first, p);
      }    
    } 
  }

  void 
  RoutingTable::AddHostRoute (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table)
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
        NS_LOG_DEBUG ("Added a new Host Route to Main Table (without expiration)" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

        RoutingTableEntry* route1 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
        route1->SetValidity (routingTableEntry->GetValidity ());
        route1->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        route1->SetRouteType (eslr::PRIMARY);
        route1->SetMetric (routingTableEntry->GetMetric ());
        route1->SetRouteChanged (true); 

        m_mainRoutingTable.push_back (std::make_pair (route1,EventId ()));
      }
      else
      {
        // This part is added to handle host route records which are receiving fomr the netibors
        NS_LOG_DEBUG ("Added a new Host Route to Main Table (with expiration)" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

        RoutingTableEntry* route2 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
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

        Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 10.0));
        EventId invalidateEvent = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, route2, p);

        m_mainRoutingTable.push_back (std::make_pair (routingTableEntry,invalidateEvent));
      }
    }
    else if (table == eslr::BACKUP)
    {
      if (settlingTime.GetSeconds () != 0)
      {
        // Add a route to the backup table as the primary route
        NS_LOG_DEBUG ("Added a new Host Route to Backup Table and schedule an event to move it to main table after settling time expires" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

        RoutingTableEntry* route4 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
        route4->SetValidity (routingTableEntry->GetValidity ());
        route4->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        route4->SetRouteType (eslr::PRIMARY);
        route4->SetMetric (routingTableEntry->GetMetric ());
        route4->SetRouteChanged (true); 

        Time delay = settlingTime + Seconds (m_rng->GetValue (0.0, 5.0));
        EventId moveToMainEvent = Simulator::Schedule (delay, &RoutingTable::MoveToMain, this, route4, invalidateTime, deleteTime, settlingTime);

        m_backupRoutingTable.push_front (std::make_pair (routingTableEntry,moveToMainEvent));
      }
      else if (settlingTime.GetSeconds () == 0)
      {
        // Add a route to the backup table as the secondary route
        NS_LOG_DEBUG ("Added a new Host Route to Backup Table and schedule an event to invalidate it" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

        RoutingTableEntry* route3 = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
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

        Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 10.0));
        EventId invalidateEvent = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, route3, p);

        m_backupRoutingTable.push_front (std::make_pair (routingTableEntry,invalidateEvent));
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
        if ((it->first->GetDestNetwork () == routingTableEntry->GetDestNetwork ()) && 
            (it->first->GetDestNetworkMask () == routingTableEntry->GetDestNetworkMask ()) &&
            (it->first->GetGateway () == routingTableEntry->GetGateway ()) &&
            (it->first->GetValidity () == eslr::INVALID || it->first->GetValidity () == eslr::DISCONNECTED))
        {
          //delete routingTableEntry;
          it->second.Cancel ();
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
        if ((it->first->GetDestNetwork () == routingTableEntry->GetDestNetwork ()) && 
            (it->first->GetDestNetworkMask () == routingTableEntry->GetDestNetworkMask ()) &&
            (it->first->GetGateway () == routingTableEntry->GetGateway ()) &&
            (it->first->GetValidity () == eslr::INVALID || it->first->GetValidity () == eslr::DISCONNECTED))
        {
          //delete routingTableEntry;
          it->second.Cancel ();
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

    bool retVal = false;

    if (param.table == eslr::MAIN)
    {
      RoutesI mainRoute, primaryRoute, secondaryRoute;

      // Main Route (Which is in the Main Table)
      bool foundInMain = FindValidRouteRecord (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), mainRoute, eslr::MAIN);

      // Primary route (Main route's agent in the Backup Table)
      bool foundPrimaryRoute = FindRouteInBackup (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), primaryRoute, eslr::PRIMARY);

      // Backup Route (the secondary route for the main route, which is in the Backup Table)
      bool foundBackupRoute = FindRouteInBackup (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), secondaryRoute, eslr::SECONDARY);

      if (!foundInMain)
      {
        NS_LOG_DEBUG ("No route found in the Main Table for the destination " << routingTableEntry->GetDestNetwork () << "/" << routingTableEntry->GetDestNetworkMask () << " Returns false.");      
        return (retVal=false);
      }

      // if the main route is invalidated because of the route expiration based on the expiration time,
      if (param.invalidateType == eslr::EXPIRE)
      {
				// if the cost of backup route is smaller than that of the primary route, 
        // update the main route and re-schedule it for invalidation.
				// meanwhile update the primary route according to the main route.
				// Finally, delete the backup route and let another backup route to come.
				
				NS_LOG_DEBUG ("The main route " << routingTableEntry->GetDestNetwork () << "/" << routingTableEntry->GetDestNetworkMask () << " is expiring."); 
        if (foundBackupRoute && (primaryRoute->first->GetMetric () > secondaryRoute->first->GetMetric ()))
        {   
          RoutingTableEntry* m_route = new RoutingTableEntry (secondaryRoute->first->GetDestNetwork (), secondaryRoute->first->GetDestNetworkMask (), secondaryRoute->first->GetGateway (), secondaryRoute->first->GetInterface ());
          m_route->SetValidity (eslr::VALID);
          m_route->SetSequenceNo (secondaryRoute->first->GetSequenceNo ());
          m_route->SetRouteType (eslr::PRIMARY);
          m_route->SetMetric (secondaryRoute->first->GetMetric ());
          m_route->SetRouteChanged (true);
          
          RoutingTableEntry* p_route = new RoutingTableEntry (secondaryRoute->first->GetDestNetwork (), secondaryRoute->first->GetDestNetworkMask (), secondaryRoute->first->GetGateway (), secondaryRoute->first->GetInterface ());
          p_route->SetValidity (eslr::VALID);
          p_route->SetSequenceNo (secondaryRoute->first->GetSequenceNo ());
          p_route->SetRouteType (eslr::PRIMARY);
          p_route->SetMetric (secondaryRoute->first->GetMetric ());
          p_route->SetRouteChanged (true);
          
          NS_LOG_DEBUG ("A low cost Backup Route " << *m_route << " is found in the Backup table. Update both Main and Primary Routes.");
                    
          mainRoute->second.Cancel ();
          m_mainRoutingTable.erase (mainRoute);
          primaryRoute->second.Cancel ();
          m_backupRoutingTable.erase (primaryRoute);
          
          AddRoute (m_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::MAIN);
          AddRoute (p_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::BACKUP);

          secondaryRoute->second.Cancel (); 
          m_backupRoutingTable.erase (secondaryRoute);
                  
          return (retVal = true);      
        }

        // If the cost of the primary route is lesser or equal than that of the main route.
        // update the main route.
        if (mainRoute->first->GetMetric () >= primaryRoute->first->GetMetric ())
        {
          RoutingTableEntry* m_route = new RoutingTableEntry (primaryRoute->first->GetDestNetwork (), primaryRoute->first->GetDestNetworkMask (), primaryRoute->first->GetGateway (), primaryRoute->first->GetInterface ());
          m_route->SetValidity (eslr::VALID);
          m_route->SetSequenceNo (primaryRoute->first->GetSequenceNo ());
          m_route->SetRouteType (eslr::PRIMARY);
          m_route->SetMetric (primaryRoute->first->GetMetric ());
          m_route->SetRouteChanged (true);
          
          RoutingTableEntry* p_route = new RoutingTableEntry (primaryRoute->first->GetDestNetwork (), primaryRoute->first->GetDestNetworkMask (), primaryRoute->first->GetGateway (), primaryRoute->first->GetInterface ());
          p_route->SetValidity (eslr::VALID);
          p_route->SetSequenceNo (primaryRoute->first->GetSequenceNo ());
          p_route->SetRouteType (eslr::PRIMARY);
          p_route->SetMetric (primaryRoute->first->GetMetric ());
          p_route->SetRouteChanged (true);

          NS_LOG_DEBUG ("The Primary route's " << *m_route << " cost is low. Update both Main and Primary Routes.");
          
          mainRoute->second.Cancel ();
          m_mainRoutingTable.erase (mainRoute);
          primaryRoute->second.Cancel ();
          m_backupRoutingTable.erase (primaryRoute);
          
          AddRoute (m_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::MAIN);
          AddRoute (p_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::BACKUP);
          
          return (retVal = true);      
          
        }                
        else if (mainRoute->first->GetMetric () < primaryRoute->first->GetMetric ())
        {
          // As the cost of the primary route is higher than that of the main route,
          // and as there are no other option, 
          // update the main route and the primary route according to the backup route.
          if (foundBackupRoute && (secondaryRoute->first->GetSequenceNo () > mainRoute->first->GetSequenceNo ()))
          {
            RoutingTableEntry* m_route = new RoutingTableEntry (secondaryRoute->first->GetDestNetwork (), secondaryRoute->first->GetDestNetworkMask (), secondaryRoute->first->GetGateway (), secondaryRoute->first->GetInterface ());
            m_route->SetValidity (eslr::VALID);
            m_route->SetSequenceNo (secondaryRoute->first->GetSequenceNo ());
            m_route->SetRouteType (eslr::PRIMARY);
            m_route->SetMetric (secondaryRoute->first->GetMetric ());
            m_route->SetRouteChanged (true);
            
            RoutingTableEntry* p_route = new RoutingTableEntry (secondaryRoute->first->GetDestNetwork (), secondaryRoute->first->GetDestNetworkMask (), secondaryRoute->first->GetGateway (), secondaryRoute->first->GetInterface ());
            p_route->SetValidity (eslr::VALID);
            p_route->SetSequenceNo (secondaryRoute->first->GetSequenceNo ());
            p_route->SetRouteType (eslr::PRIMARY);
            p_route->SetMetric (secondaryRoute->first->GetMetric ());
            p_route->SetRouteChanged (true);   
            
            NS_LOG_DEBUG ("As the cost of the Primary Route is Higher, founded Backup route " << *m_route << " is used to update both Main and Primary Routes.");
              
            mainRoute->second.Cancel ();
            m_mainRoutingTable.erase (mainRoute);
            primaryRoute->second.Cancel ();
            m_backupRoutingTable.erase (primaryRoute);
            
            AddRoute (m_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::MAIN);
            AddRoute (p_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::BACKUP);

            secondaryRoute->second.Cancel (); 
            m_backupRoutingTable.erase (secondaryRoute);
            
            return (retVal = true);                  
          }
          else
          {
            // As there are no up-to-date record found in the backup table,
            // and as the primary route is also worse compared to the main route, 
            // there is no other option but invalidate and delete both main and primary route.
            NS_LOG_DEBUG ("There are not good routes matches the Main route. Invalidate and delete the both Main and Primary Route!.");           
            Time delay = param.deleteTime + Seconds (m_rng->GetValue (0.0, 5.0));

						if (foundInMain)
						{
						mainRoute->first->SetValidity (eslr::INVALID);
            mainRoute->first->SetRouteChanged (true);
            mainRoute->second.Cancel ();
            mainRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, mainRoute->first, eslr::MAIN);

            }
						if (foundPrimaryRoute)
						{
            primaryRoute->first->SetValidity (eslr::INVALID);
            primaryRoute->first->SetRouteChanged (true);
            primaryRoute->second.Cancel ();
   
            primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, primaryRoute->first, eslr::BACKUP);
						}
            return (retVal = true);            
          }             
        }
      }
      else if (param.invalidateType == eslr::BROKEN) // if a route is invalidate cos, it is broken
      {
        // if the route is not about a locally connected network,
        // and an up-to-date backup route found,
        // update the main and primary routes according to the secondary route.
        
				NS_LOG_DEBUG ("The main route " << routingTableEntry->GetDestNetwork () << "/" << routingTableEntry->GetDestNetworkMask () << " is broken.");
				        
        if (foundBackupRoute && 
            (secondaryRoute->first->GetSequenceNo () > mainRoute->first->GetSequenceNo ()) && 
            (routingTableEntry->GetGateway () !=  Ipv4Address::GetZero ()))
        {
          RoutingTableEntry* m_route = new RoutingTableEntry (secondaryRoute->first->GetDestNetwork (), secondaryRoute->first->GetDestNetworkMask (), secondaryRoute->first->GetGateway (), secondaryRoute->first->GetInterface ());
          m_route->SetValidity (eslr::VALID);
          m_route->SetSequenceNo (secondaryRoute->first->GetSequenceNo ());
          m_route->SetRouteType (eslr::PRIMARY);
          m_route->SetMetric (secondaryRoute->first->GetMetric ());
          m_route->SetRouteChanged (true);
          
          RoutingTableEntry* p_route = new RoutingTableEntry (secondaryRoute->first->GetDestNetwork (), secondaryRoute->first->GetDestNetworkMask (), secondaryRoute->first->GetGateway (), secondaryRoute->first->GetInterface ());
          p_route->SetValidity (eslr::VALID);
          p_route->SetSequenceNo (secondaryRoute->first->GetSequenceNo ());
          p_route->SetRouteType (eslr::PRIMARY);
          p_route->SetMetric (secondaryRoute->first->GetMetric ());
          p_route->SetRouteChanged (true);   

          NS_LOG_DEBUG ("A Backup Route " << *m_route << " is found in the Backup table. Update both Main and Primary Routes.");
                    
          mainRoute->second.Cancel ();
          m_mainRoutingTable.erase (mainRoute);
          primaryRoute->second.Cancel ();
          m_backupRoutingTable.erase (primaryRoute);
          
          AddRoute (m_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::MAIN);
          AddRoute (p_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::BACKUP);                

          secondaryRoute->second.Cancel (); 
          m_backupRoutingTable.erase (secondaryRoute);

          return (retVal = true);          
        }
        else
        {
          // as the broken route is about either locally connected network, 
          // or there is no up-to-date backup route found,
          // schedule to delete the relevant route record by stating the route is Disconnected. 
          
          NS_LOG_DEBUG ("As the route is broken, and there is no Backup Route presents in the backup table,  mark as disconnected and delete the both Main and Primary Route!.");                   
          if (foundInMain)
          {
            mainRoute->first->SetValidity (eslr::DISCONNECTED);
            mainRoute->first->SetRouteChanged (true);

            mainRoute->second.Cancel ();

            Time delay = param.deleteTime; + Seconds (m_rng->GetValue (0.0, 5.0));
            mainRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, mainRoute->first, eslr::MAIN); 
            //DeleteRoute (mainRoute->first, eslr::MAIN);
          }
          if (foundPrimaryRoute)
          {          
            primaryRoute->first->SetValidity (eslr::DISCONNECTED);
            primaryRoute->first->SetRouteChanged (true);

            primaryRoute->second.Cancel ();
              
            Time delay = param.deleteTime; + Seconds (m_rng->GetValue (0.0, 5.0));
            primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, primaryRoute->first, eslr::BACKUP);
            //DeleteRoute (primaryRoute->first, eslr::BACKUP);
          }
          
          return (retVal = true);
        }
      }
    }
    else if (param.table == eslr::BACKUP)
    {
      RoutesI bkupRoute;
      
      NS_ASSERT_MSG (routingTableEntry->GetDestNetwork () != Ipv4Address (), "Valid forwarding address is not found");
      bool foundInBackup = FindRouteRecord (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), bkupRoute, eslr::BACKUP);

      if (foundInBackup)
      {      
        if (bkupRoute->first->GetRouteType () == eslr::SECONDARY)
        {
          NS_LOG_DEBUG ("The Backup Route " << routingTableEntry->GetDestNetwork () << "/" << routingTableEntry->GetDestNetworkMask () << " is expiring.");
          
          RoutesI mainRoute, primaryRoute;

          bool foundInMain = FindValidRouteRecord (bkupRoute->first->GetDestNetwork (), bkupRoute->first->GetDestNetworkMask (), mainRoute, eslr::MAIN);

          FindRouteInBackup (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), primaryRoute, eslr::PRIMARY);
          
          // if the backup route is going to expire,
          // and the main route is also about to expire,
          // and the cost of the backup route is smaller than both main and primary routes,
          // and the backup route is up-to-date route,
          // update the main and primary routes according to the backup route.
          if (foundInMain && 
              (Simulator::GetDelayLeft (mainRoute->second) < (param.invalidateTime/4)) && 
              (mainRoute->first->GetMetric () > bkupRoute->first->GetMetric ()) && 
              (primaryRoute->first->GetMetric () > bkupRoute->first->GetMetric ()) && 
              (mainRoute->first->GetSequenceNo () < bkupRoute->first->GetSequenceNo ()) &&
              (param.invalidateType == eslr::EXPIRE))
          {
            RoutingTableEntry* m_route = new RoutingTableEntry (bkupRoute->first->GetDestNetwork (), bkupRoute->first->GetDestNetworkMask (), bkupRoute->first->GetGateway (), bkupRoute->first->GetInterface ());
            m_route->SetValidity (eslr::VALID);
            m_route->SetSequenceNo (bkupRoute->first->GetSequenceNo ());
            m_route->SetRouteType (eslr::PRIMARY);
            m_route->SetMetric (bkupRoute->first->GetMetric ());
            m_route->SetRouteChanged (true);
            
            RoutingTableEntry* p_route = new RoutingTableEntry (bkupRoute->first->GetDestNetwork (), bkupRoute->first->GetDestNetworkMask (), bkupRoute->first->GetGateway (), bkupRoute->first->GetInterface ());
            p_route->SetValidity (eslr::VALID);
            p_route->SetSequenceNo (bkupRoute->first->GetSequenceNo ());
            p_route->SetRouteType (eslr::PRIMARY);
            p_route->SetMetric (bkupRoute->first->GetMetric ());
            p_route->SetRouteChanged (true); 
            
            NS_LOG_DEBUG ("The Main Route " << *mainRoute->first << " is also about to expire. As I am a better route, Update the Main and Primary Route.");
                      
            mainRoute->second.Cancel ();
            m_mainRoutingTable.erase (mainRoute);
            primaryRoute->second.Cancel ();
            m_backupRoutingTable.erase (primaryRoute);
            
            AddRoute (m_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::MAIN);
            AddRoute (p_route, param.invalidateTime, param.deleteTime, Seconds (0), eslr::BACKUP);

            bkupRoute->second.Cancel (); 
            m_backupRoutingTable.erase (bkupRoute);

            return (retVal = true);
          }
          else
          {
            // as the backup route is not anymore valid or,
            // the backup route is about a broken link,
            // delete the backup route.

            NS_LOG_DEBUG ("Invalidate and Delete the Backup Route!.");     
                 
            bkupRoute->first->SetValidity (eslr::DISCONNECTED);
            bkupRoute->first->SetRouteChanged (true); 

            bkupRoute->second.Cancel (); 
              
            Time delay = param.deleteTime + Seconds (m_rng->GetValue (0.0, 5.0));           
            bkupRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, bkupRoute->first, eslr::BACKUP);            
          	return retVal = true;
					}
        }
        else if (bkupRoute->first->GetRouteType () == eslr::PRIMARY)
        {
          // In ESLR, 
          // primary routes are never invalidated. 
          // such routes are invalidated along with the main routes.
          // even accidentally they do, in this point, that will be corrected.

          bkupRoute->second.Cancel (); 
          bkupRoute->second = EventId ();  
          
          return (retVal = true);
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
  RoutingTable::FindValidRouteRecord (Ipv4Address destination, Ipv4Mask netMask, Ipv4Address gateway, RoutesI &retRoutingTableEntry, eslr::Table table)
  {
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
            (it->first->GetGateway () == gateway) &&
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

  bool 
  RoutingTable::FindRoutesForInterface (uint32_t interface, RoutingTable::RoutingTableInstance &matchedRouteEntries, eslr::Table table)
  {
    bool retVal = false;

    if (table == eslr::MAIN)
    {
      for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
      {
        if (it->first->GetInterface () == interface)
        {
          matchedRouteEntries.push_front(std::make_pair (it->first,it->second));
          retVal = true;
        }
      }
    }
    else if (table == eslr::BACKUP)
    {
      for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
      {
        if (it->first->GetInterface () == interface)
        {
          matchedRouteEntries.push_front(std::make_pair (it->first,it->second));
          retVal = true;
        }
      } 
    }
    return retVal;
  }

  bool 
  RoutingTable::FindRoutesForGateway (Ipv4Address gateway, RoutingTable::RoutingTableInstance &matchedRouteEntries, eslr::Table table)
  {
    bool retVal = false;

    if (table == eslr::MAIN)
    {
      for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
      {
        if ((it->first->GetGateway () == gateway) && (it->first->GetValidity () == eslr::VALID))
        {
          matchedRouteEntries.push_front(std::make_pair (it->first,it->second));
          retVal = true;
        }
      }

    }
    else if (table == eslr::BACKUP)
    {
      for (RoutesI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
      {
        if ((it->first->GetGateway () == gateway))// && (it->first->GetValidity () == eslr::VALID))
        {
          matchedRouteEntries.push_front(std::make_pair (it->first,it->second));
          retVal = true;
        }
      } 
    }
    return retVal;
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

  bool 
  RoutingTable::UpdateNetworkRoute (RoutingTableEntry *routingTableEntry, Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table)
  {
    NS_LOG_FUNCTION (this << *routingTableEntry);
    
    bool retVal = false;

    if (table == eslr::MAIN)
    {
      RoutesI mainRoute, primaryRoute;

      bool foundInMain = FindRouteRecord (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), mainRoute, table);

      FindRouteInBackup (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), primaryRoute, eslr::PRIMARY);

      // For the routes learned by neighbors, 
      // update both Main and Primary route in Main and backup routing table respectively.
      if (foundInMain && (routingTableEntry->GetGateway () != Ipv4Address::GetZero ()))
      {
        NS_LOG_DEBUG ("Update the Main Route " << *mainRoute->first << " and the Primary Route"); 
        RoutingTableEntry* m_route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
        m_route->SetValidity (routingTableEntry->GetValidity ());
        m_route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        m_route->SetRouteType (eslr::PRIMARY);
        m_route->SetMetric (routingTableEntry->GetMetric ());
        m_route->SetRouteChanged (true);
        
        RoutingTableEntry* p_route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
        p_route->SetValidity (routingTableEntry->GetValidity ());
        p_route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        p_route->SetRouteType (eslr::PRIMARY);
        p_route->SetMetric (routingTableEntry->GetMetric ());
        p_route->SetRouteChanged (true);
        
        mainRoute->second.Cancel ();
        m_mainRoutingTable.erase (mainRoute);
        primaryRoute->second.Cancel ();
        m_backupRoutingTable.erase (primaryRoute);
        
        AddRoute (m_route, invalidateTime, deleteTime, Seconds (0), eslr::MAIN);
        AddRoute (p_route, invalidateTime, deleteTime, Seconds (0), eslr::BACKUP);        

//        delete mainRoute->first;
//        mainRoute->first = m_route;
//        
//        delete primaryRoute->first;
//        primaryRoute->first = p_route;        

//        //if (mainRoute->second.IsRunning ())
//          mainRoute->second.Cancel ();   

//        invalidateParams  p;
//        p.invalidateTime = invalidateTime;
//        p.deleteTime = deleteTime;
//        p.settlingTime = settlingTime;
//        p.invalidateType = eslr::EXPIRE;
//        p.table = eslr::MAIN;     

//        Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 10.0));          
//        mainRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, mainRoute->first, p);

//        //if (primaryRoute->second.IsRunning ())
//          primaryRoute->second.Cancel (); 
//        
//        primaryRoute->second  = EventId (); 

        return retVal = true;
      }
      else if (foundInMain && (routingTableEntry->GetGateway () == Ipv4Address::GetZero ()))
      {
        // Locally connected routes are only updated for the route information
        NS_LOG_DEBUG ("As the update is about a locally connected network, " <<  routingTableEntry->GetDestNetwork () << " Update only the Main Route ");      
        
        RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
        route->SetValidity (routingTableEntry->GetValidity ());
        route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        route->SetRouteType (eslr::PRIMARY);
        route->SetMetric (routingTableEntry->GetMetric ());
        route->SetRouteChanged (routingTableEntry->GetRouteChanged ());

        delete mainRoute->first;
        mainRoute->first = route;

        return retVal = true;
      }
    }
    else if (table == eslr::BACKUP)
    {    
      RoutesI primaryRoute, secondaryRoute;
      bool foundPrimary, foundSecondary;

      foundPrimary = FindRouteInBackup (routingTableEntry->GetDestNetwork (), 
                                        routingTableEntry->GetDestNetworkMask (), 
                                        primaryRoute, eslr::PRIMARY);
      foundSecondary = FindRouteInBackup (routingTableEntry->GetDestNetwork (), 
                                          routingTableEntry->GetDestNetworkMask (), 
                                          secondaryRoute, eslr::SECONDARY);
  
      if (routingTableEntry->GetRouteType () == eslr::PRIMARY)
      {        
        if (foundPrimary)
        {
          // Update the primary route and,
          // schedule and event to add the updated route after a settling time.
          RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());

          route->SetValidity (routingTableEntry->GetValidity ());
          route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
          route->SetRouteType (eslr::PRIMARY);
          route->SetMetric (routingTableEntry->GetMetric ());
          route->SetRouteChanged (true);

          delete primaryRoute->first;
          primaryRoute->first = route;

          //primaryRoute->second.Cancel ();  
          
          NS_LOG_DEBUG ("As the update is about the Primary Route, Update it and schedule and event to move it to the Main table after setting time");
          
          //ToDo:  has the check this
          
          //Time delay = settlingTime + Seconds (m_rng->GetValue (0.0, 5.0));          
          //primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::MoveToMain, this, primaryRoute->first, invalidateTime, deleteTime, settlingTime);

          return retVal = true; 
        }
        else
          return retVal = false;
      }
      else if (routingTableEntry->GetRouteType () == eslr::SECONDARY)
      {
        if (foundSecondary)
        {
          // update the secondary record and,
          // schedule and event to expire the route.
          RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
          route->SetValidity (routingTableEntry->GetValidity ());
          route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
          route->SetRouteType (eslr::SECONDARY);
          route->SetMetric (routingTableEntry->GetMetric ());
          route->SetRouteChanged (true);

          delete secondaryRoute->first;
          secondaryRoute->first = route;

          secondaryRoute->second.Cancel (); 

          NS_LOG_DEBUG ("As the update is about the Backup Route, Update it and schedule and event to invalidate it after invalidate time");
          
          invalidateParams  p;
          p.invalidateTime = invalidateTime;
          p.deleteTime = deleteTime;
          p.settlingTime = settlingTime;
          p.invalidateType = eslr::EXPIRE;
          p.table = eslr::BACKUP;      

          Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 10.0));          
          secondaryRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, secondaryRoute->first, p);     
            
          return retVal = true;            
        }
        else 
          return retVal = false;
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
      // Note: In the case of Main routing table, at the time returning the routing table, 
      // a newly created separate instance is returned. However, in order to manage memory properly, 
      // table instance has to be cleared at the place where requesting the main routing table.
      
      NS_LOG_DEBUG ("Create a fresh instance of the Main table and return");
      
      RoutingTableEntry* route;
      for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
      {
        route  =  new RoutingTableEntry (it->first->GetDestNetwork (), it->first->GetDestNetworkMask (), it->first->GetGateway (), it->first->GetInterface ());    
          route->SetValidity (it->first->GetValidity ());
          route->SetSequenceNo (it->first->GetSequenceNo ());
          route->SetRouteType (it->first->GetRouteType ());
          route->SetMetric (it->first->GetMetric ());
          route->SetRouteChanged (it->first->GetRouteChanged ());
          
        //instance.push_front(std::make_pair (it->first, it->second));
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
      *os << "Destination         Gateway        If  Seq#    Metric  Validity     Changed Expire in (s)" << '\n';
      *os << "------------------  -------------  --  ------  ------  --------     ------- -------------" << '\n';

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
          *os << std::setiosflags (std::ios::left) << std::setw (15) << gateway.str ();
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
      *os << "Destination         Gateway        If  Seq#    Metric  Validity      Pri/Sec Next Event (s)" << '\n';
      *os << "------------------  -------------  --  ------  ------  ------------  ------- --------------" << '\n';

      for (RoutesCI it = m_backupRoutingTable.begin ();  it!= m_backupRoutingTable.end (); it++)
      {

        RoutingTableEntry *route = it->first;

        std::ostringstream dest, gateway, validity, preSec;

        dest << route->GetDestNetwork () << "/" << int (route->GetDestNetworkMask ().GetPrefixLength ());
        *os << std::setiosflags (std::ios::left) << std::setw (20) << dest.str ();
        gateway << route->GetGateway ();
        *os << std::setiosflags (std::ios::left) << std::setw (15) << gateway.str ();
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
  RoutingTable::InvalidateRoutesForGateway (Ipv4Address gateway,  Time invalidateTime, Time deleteTime, Time settlingTime)
  {
    NS_LOG_FUNCTION (this << gateway);
    RoutingTableInstance routesInMain, routesInBackup;
    bool foundInMain, foundInBackup;
    
    foundInMain = FindRoutesForGateway (gateway, routesInMain, eslr::MAIN);
    foundInBackup = FindRoutesForGateway (gateway, routesInBackup, eslr::BACKUP);

    if (foundInMain)
    {
      NS_LOG_DEBUG ("Invalidate all routes in the Main table that has the gateway as " << gateway);    
      for (RoutesI it = routesInMain.begin ();  it!= routesInMain.end (); it++)
      {
       //if (it->second.IsRunning ())
        it->second.Cancel ();

        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::BROKEN;
        p.table = eslr::MAIN;     

        //Time delay = /*Seconds (invalidateTime.GetSeconds () / 10) + */Seconds (m_rng->GetValue (0.0, 2.5));
        //it->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, it->first, p);
        InvalidateRoute (it->first, p);
      }
    }
    if (foundInBackup)
    {
      NS_LOG_DEBUG ("Invalidate all routes in the Backup table that has the gateway as " << gateway);        
      for (RoutesI it = routesInBackup.begin ();  it!= routesInBackup.end (); it++)
      {
       //if (it->second.IsRunning ())
        it->second.Cancel ();

        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::BROKEN;
        p.table = eslr::BACKUP;     

        //Time delay = /*Seconds (invalidateTime.GetSeconds () / 10) + */Seconds (m_rng->GetValue (0.0, 2.5));
        //it->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, it->first, p);
        InvalidateRoute (it->first, p);
      }
    }
  }

  void 
  RoutingTable::InvalidateRoutesForInterface (uint32_t interface,  Time invalidateTime, Time deleteTime, Time settlingTime)
  {
    NS_LOG_FUNCTION (this << interface);
    RoutingTableInstance routesInMain, routesInBackup;
    bool foundInMain, foundInBackup;
    
    foundInMain = FindRoutesForInterface (interface, routesInMain, eslr::MAIN);
    foundInBackup = FindRoutesForInterface (interface, routesInBackup, eslr::BACKUP);

    if (foundInMain)
    {
      NS_LOG_DEBUG ("Invalidate all routes in the Main table that refers the interface " << interface);        
      for (RoutesI it = routesInMain.begin ();  it!= routesInMain.end (); it++)
      {
       //if (it->second.IsRunning ())
        it->second.Cancel ();

        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::BROKEN;
        p.table = eslr::MAIN;    

        Time delay = /*Seconds (invalidateTime.GetSeconds () / 10) + */Seconds (m_rng->GetValue (0.0, 2.5));
        it->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, it->first, p);
        //InvalidateRoute (it->first, p);
      }
    }
    if (foundInBackup)
    {
      NS_LOG_DEBUG ("Invalidate all routes in the Backup table that refers the interface " << interface);   
      for (RoutesI it = routesInBackup.begin ();  it!= routesInBackup.end (); it++)
      {
       //if (it->second.IsRunning ())
        it->second.Cancel ();

        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::BROKEN;
        p.table = eslr::BACKUP;   

        Time delay = /*Seconds (invalidateTime.GetSeconds () / 10) + */Seconds (m_rng->GetValue (0.0, 2.5));
        it->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, it->first, p);
        //InvalidateRoute (it->first, p);        
      }
    }
  }

  bool 
  RoutingTable::InvalidateBrokenRoute (Ipv4Address destAddress, Ipv4Mask destMask, Ipv4Address gateway, Time invalidateTime, Time deleteTime, Time settlingTime, eslr::Table table)
  {
    bool retVal = false;
    RoutingTable::RoutesI foundRoute;
    bool found = FindValidRouteRecord (destAddress, destMask, gateway, foundRoute, table);

    if (found)
    {
      //if (foundRoute->second.IsRunning ())
        foundRoute->second.Cancel ();

        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::BROKEN;
        p.table = table;   

        NS_LOG_DEBUG ("Invalidate the broken route " << foundRoute->first << " of the table " << table);
         
        //Time delay = Seconds (invalidateTime.GetSeconds () / 10) + Seconds (m_rng->GetValue (0.0, 2.5));
        //foundRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, foundRoute->first, p); 
        InvalidateRoute (foundRoute->first, p);
      retVal = true;   
    }
    return retVal;
  }
  
  void 
  RoutingTable::ToggleRouteChanged ()
  {
    NS_LOG_FUNCTION (this);
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      it->first->SetRouteChanged (false);
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


std::ostream & operator << (std::ostream& os, const RoutingTableEntry& rte)
{
  os << static_cast<const Ipv4RoutingTableEntry &>(rte);
  os << ", metric: " << int (rte.GetMetric ()) << ", tag: " << int (rte.GetRouteTag ());

  return os;
}
}// end of eslr
}// end of ns3
