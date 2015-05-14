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
 * \brief this implementation is basically implemented for route table managements.
 * route tables are defined as main and backup. While the main routing table keeps the topology table,
 * backup table maintain two records for one destination. It maintains a reference to the route entry in the
 * main table and another record, which has the next best cost.
 * depending on the table, update, delete and invalidate methods are getting deffer. 
 * For both tables, one method of actions is implemented. At the calling time, users need to specify
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
    if (table == eslr::MAIN)
    {
      // create new route and add to the main table
      // route will be invalidated after timeout time
      NS_LOG_FUNCTION ("Added a new Route to Main Table " << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

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
        NS_LOG_FUNCTION ("Added a new Route to Backup Table and schedule an event to move it to main table after settling time expires " << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

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
        NS_LOG_FUNCTION ("Added a new Route to Backup Table and schedule an event to invalidate it after invalidate time expires" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

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
    // This method will be triggered at the settling time event expires
    // Route add and update methods will call this method
    // This method will then call the add or update route methods respectively.
    NS_LOG_FUNCTION ("Move a Route to Main Table " << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

    RoutesI existingRoute;

    bool foundInMain = FindRouteRecord (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), existingRoute, eslr::MAIN);

    if (foundInMain)
    {
      UpdateNetworkRoute (routingTableEntry, invalidateTime, deleteTime, settlingTime, eslr::MAIN);
    }
    else
    {
      // As the route is added to the main table via backup table, the hold down time is 0
      AddRoute (routingTableEntry, invalidateTime, deleteTime, Seconds(0), eslr::MAIN);
    } 

    // Mark the route as PRIMARY
    NS_LOG_FUNCTION ("Update the route record in the Backup table " );

    for (RoutesI it = m_backupRoutingTable.begin (); it != m_backupRoutingTable.end (); it++)
    {
      if (it->first == routingTableEntry)
      {
        it->first->SetRouteType (eslr::PRIMARY);
        it->second.Cancel ();
        it->second = EventId (); // Do not invalidate until main table decides
      }
      // Marking the moved route as PRIMARY
      //if ((it->first->GetDestNetwork () == routingTableEntry->GetDestNetwork ()) && 
      //    (it->first->GetDestNetworkMask () == routingTableEntry->GetDestNetworkMask ()) &&
      //    (it->first->GetGateway () == routingTableEntry->GetGateway ()))
      //{
      //  it->first->SetRouteType (eslr::PRIMARY);
      //  it->second.Cancel ();
      //  it->second = EventId (); // Do not invalidate until main table decides
      //}
      else if ((it->first->GetDestNetwork () == routingTableEntry->GetDestNetwork ()) && 
               (it->first->GetDestNetworkMask () == routingTableEntry->GetDestNetworkMask ()) &&
               (it->first->GetGateway () != routingTableEntry->GetGateway ()))
      {
        // Marking rest of the routes as SECONDARY
        it->first->SetRouteType (eslr::SECONDARY);
        //if(it->second.IsRunning ())
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
    NS_LOG_FUNCTION (this << "Added a new Host Routes" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));
    if (table == eslr::MAIN)
    {
      if ((invalidateTime.GetSeconds () == 0) &&
          (deleteTime.GetSeconds ()  == 0) &&
          (settlingTime.GetSeconds () == 0))        
      {
        // routes about the local interfaces are added using this part
        NS_LOG_FUNCTION ("Added a new Host Route to Main Table (without expiration)" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

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
        NS_LOG_FUNCTION ("Added a new Host Route to Main Table (with expiration)" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

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
        // add the record to the backup table and move it to the main table after settling time
        NS_LOG_FUNCTION ("Added a new Route to Backup Table and schedule an event to move it to main table after settling time expires " << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

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
        // add to the backup table as the secondary route
        NS_LOG_FUNCTION ("Added a new Route to Backup Table and schedule an event to invalidate it after invalidate time expires" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

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
    NS_LOG_FUNCTION (this << "Delete Routes" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));
    bool retVal = false;
    if (table == eslr::MAIN)
    {
      NS_LOG_FUNCTION (this << "Delete Routes in main table");
      for (RoutesI it = m_mainRoutingTable.begin (); it != m_mainRoutingTable.end (); it++)
      {
        if ((it->first->GetDestNetwork () == routingTableEntry->GetDestNetwork ()) && 
            (it->first->GetDestNetworkMask () == routingTableEntry->GetDestNetworkMask ()) &&
            (it->first->GetGateway () == routingTableEntry->GetGateway ()) &&
            (it->first->GetValidity () == eslr::INVALID))
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
      NS_LOG_FUNCTION (this << "Delete Routes in backup table");
      for (RoutesI it = m_backupRoutingTable.begin (); it != m_backupRoutingTable.end (); it++)
      {
        if ((it->first->GetDestNetwork () == routingTableEntry->GetDestNetwork ()) && 
            (it->first->GetDestNetworkMask () == routingTableEntry->GetDestNetworkMask ()) &&
            (it->first->GetGateway () == routingTableEntry->GetGateway ()) &&
            (it->first->GetValidity () == eslr::INVALID))
        {
          //delete routingTableEntry;
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
    NS_LOG_FUNCTION (this << "Invalidating the routes in both routing table" << routingTableEntry->GetDestNetwork () << "/" << int (routingTableEntry->GetDestNetworkMask ().GetPrefixLength ()));

    bool retVal = false;

    if (param.table == eslr::MAIN)
    {
      RoutesI mainRoute, primaryRoute, secondaryRoute;

      bool foundInMain = FindValidRouteRecord (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), mainRoute, eslr::MAIN);

      bool foundPrimaryRoute = FindRouteInBackup (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), primaryRoute, eslr::PRIMARY);

      bool foundBackupRoute = FindRouteInBackup (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), secondaryRoute, eslr::SECONDARY);

      if (!foundInMain)
      {
        return (retVal=false);
      }

      if (param.invalidateType == eslr::EXPIRE)
      {
				// if the cost of backup route is smaller than that of the main route, 
        // update the main route and re-shedule it for invalidation
				// meanwhile update the primary route according to the main route
				// Finally Delete the backup route and let some another backup route to be added to the backup routing table
        if (foundBackupRoute && (mainRoute->first->GetMetric () > secondaryRoute->first->GetMetric ()))
        {
          RoutingTableEntry* route = new RoutingTableEntry (secondaryRoute->first->GetDestNetwork (), secondaryRoute->first->GetDestNetworkMask (), secondaryRoute->first->GetGateway (), secondaryRoute->first->GetInterface ());
          route->SetValidity (eslr::VALID);
          route->SetSequenceNo (secondaryRoute->first->GetSequenceNo ());
          route->SetRouteType (eslr::PRIMARY);
          route->SetMetric (secondaryRoute->first->GetMetric ());
          route->SetRouteChanged (true);

          secondaryRoute->first->SetValidity (eslr::INVALID);
          secondaryRoute->first->SetRouteChanged (true);
          //if (secondaryRoute->second.IsRunning ())
          secondaryRoute->second.Cancel (); 
          m_backupRoutingTable.erase (secondaryRoute);
          //secondaryRoute->second = Simulator::Schedule (param.deleteTime, &RoutingTable::DeleteRoute, this, secondaryRoute->first, eslr::BACKUP);

          mainRoute->second.Cancel ();
          m_mainRoutingTable.erase (mainRoute);
          primaryRoute->second.Cancel ();
          m_backupRoutingTable.erase (primaryRoute);

          Time delay = Seconds (m_rng->GetValue (0.0, 5.0));
          //UpdateNetworkRoute (route, param.invalidateTime, param.deleteTime, delay, eslr::BACKUP); 
          AddRoute (route, param.invalidateTime, param.deleteTime, delay, eslr::BACKUP);
        
          return (retVal = true);      
        }

        if (mainRoute->first->GetMetric () >= primaryRoute->first->GetMetric ())
        {
          // In any case primary rouet has the low cost update than the main route, as the main route is about to expire,
					// update the main route and set it to expire. Meanwhile, reshedule the primary route not to expire.
          // at this moment settlig time is ignore, if it is expired or not, the route is added as the main route.

          RoutingTableEntry* route = new RoutingTableEntry (primaryRoute->first->GetDestNetwork (), primaryRoute->first->GetDestNetworkMask (), primaryRoute->first->GetGateway (), primaryRoute->first->GetInterface ());
          route->SetValidity (eslr::VALID);
          route->SetSequenceNo (primaryRoute->first->GetSequenceNo ());
          route->SetRouteType (eslr::PRIMARY);
          route->SetMetric (primaryRoute->first->GetMetric ());
          route->SetRouteChanged (true);

          Time delay = Seconds (m_rng->GetValue (0.0, 5.0));
          UpdateNetworkRoute (route, param.invalidateTime, param.deleteTime, delay, eslr::BACKUP);             
        }                
        else if (mainRoute->first->GetMetric () < primaryRoute->first->GetMetric ())
        {
					// as there are no option, add the existing backup route as the primary route and update the main route according to the backup route
          NS_LOG_FUNCTION (this << "Check the backup table for backup routes");

          if (foundBackupRoute && (secondaryRoute->first->GetSequenceNo () > mainRoute->first->GetSequenceNo ()))
          {
            NS_LOG_FUNCTION (this << "Set the found backup route as the primary route, and delete the backup");

            RoutingTableEntry* route = new RoutingTableEntry (secondaryRoute->first->GetDestNetwork (), secondaryRoute->first->GetDestNetworkMask (), secondaryRoute->first->GetGateway (), secondaryRoute->first->GetInterface ());
            route->SetValidity (eslr::VALID);
            route->SetSequenceNo (secondaryRoute->first->GetSequenceNo ());
            route->SetRouteType (eslr::PRIMARY);
            route->SetMetric (secondaryRoute->first->GetMetric ());
            route->SetRouteChanged (true);

            secondaryRoute->first->SetValidity (eslr::INVALID);
            secondaryRoute->first->SetRouteChanged (true);
            //if (secondaryRoute->second.IsRunning ())
            secondaryRoute->second.Cancel (); 
            m_backupRoutingTable.erase (secondaryRoute);
            //secondaryRoute->second = Simulator::Schedule (param.deleteTime, &RoutingTable::DeleteRoute, this, secondaryRoute->first, eslr::BACKUP);

            mainRoute->second.Cancel ();
            m_mainRoutingTable.erase (mainRoute);
            primaryRoute->second.Cancel ();
            m_backupRoutingTable.erase (primaryRoute);

            Time delay = Seconds (m_rng->GetValue (0.0, 5.0));
            //UpdateNetworkRoute (route, param.invalidateTime, param.deleteTime, delay, eslr::BACKUP); 
            AddRoute (route, param.invalidateTime, param.deleteTime, delay, eslr::BACKUP);
          }
          else
          {
						// as there is no backup route instead, schedule envents to delete both main and primary route in Main and Backup routing tables respectively
            mainRoute->first->SetValidity (eslr::INVALID);
            mainRoute->first->SetRouteChanged (true);

            primaryRoute->first->SetValidity (eslr::INVALID);
            primaryRoute->first->SetRouteChanged (true);

            //if (mainRoute->second.IsRunning ())
              mainRoute->second.Cancel ();

            //if (primaryRoute->second.IsRunning ())
              primaryRoute->second.Cancel ();     

            Time delay = param.deleteTime + Seconds (m_rng->GetValue (0.0, 5.0));
           
            mainRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, mainRoute->first, eslr::MAIN);

            primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, primaryRoute->first, eslr::BACKUP);
          }             
          return (retVal = true);
        }
      }
      else if (param.invalidateType == eslr::BROKEN)
      {
				// if the broken route is about a route learned from a neighbor router,
				// find the backup route and set it as the main route. Further, add it as the primary route.
        if (foundBackupRoute && 
            (secondaryRoute->first->GetSequenceNo () > mainRoute->first->GetSequenceNo ()) && 
            (routingTableEntry->GetGateway () !=  Ipv4Address::GetZero ()))
        {
          NS_LOG_FUNCTION (this << "Set the found backup route as the primary route");

          RoutingTableEntry* route = new RoutingTableEntry (secondaryRoute->first->GetDestNetwork (), secondaryRoute->first->GetDestNetworkMask (), secondaryRoute->first->GetGateway (), secondaryRoute->first->GetInterface ());
          route->SetValidity (eslr::VALID);
          route->SetSequenceNo (secondaryRoute->first->GetSequenceNo ());
          route->SetRouteType (eslr::PRIMARY);
          route->SetMetric (secondaryRoute->first->GetMetric ());
          route->SetRouteChanged (true);

          secondaryRoute->first->SetValidity (eslr::INVALID);
          secondaryRoute->first->SetRouteChanged (true);
          //if (secondaryRoute->second.IsRunning ())
          secondaryRoute->second.Cancel (); 
          m_backupRoutingTable.erase (secondaryRoute);
          //secondaryRoute->second = Simulator::Schedule (param.deleteTime, &RoutingTable::DeleteRoute, this, secondaryRoute->first, eslr::BACKUP);

          mainRoute->second.Cancel ();
          m_mainRoutingTable.erase (mainRoute);
          primaryRoute->second.Cancel ();
          m_backupRoutingTable.erase (primaryRoute);

          Time delay = Seconds (m_rng->GetValue (0.0, 5.0));
          //UpdateNetworkRoute (route, param.invalidateTime, param.deleteTime, delay, eslr::BACKUP); 
          AddRoute (route, param.invalidateTime, param.deleteTime, delay, eslr::BACKUP);
        }
        else
        {
					// since the broken route is about the local interfaces, or existing sequence number are not the latest, remove those records from both Main and Backup routing tables        
          if (foundInMain)
          {
            mainRoute->first->SetValidity (eslr::INVALID);
            mainRoute->first->SetRouteChanged (true);

            //if (mainRoute->second.IsRunning ())
              mainRoute->second.Cancel ();

            Time delay = param.deleteTime + Seconds (m_rng->GetValue (0.0, 5.0));
            mainRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, mainRoute->first, eslr::MAIN); 
          }
          if (foundPrimaryRoute)
          {          
            primaryRoute->first->SetValidity (eslr::INVALID);
            primaryRoute->first->SetRouteChanged (true);

            //if (primaryRoute->second.IsRunning ())
              primaryRoute->second.Cancel ();
              
            Time delay = param.deleteTime + Seconds (m_rng->GetValue (0.0, 5.0));
            primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, primaryRoute->first, eslr::BACKUP);
          }
        }
        return (retVal = true);
      }
    }
    else if (param.table == eslr::BACKUP)
    {
      RoutesI bkupRoute;
      bool foundInBackup = FindRouteRecord (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), bkupRoute, eslr::BACKUP);

      if (foundInBackup)
      {
        if (bkupRoute->first->GetRouteType () == eslr::SECONDARY)
        {
          RoutesI mainRoute, primaryRoute;

          bool foundInMain = FindValidRouteRecord (bkupRoute->first->GetDestNetwork (), bkupRoute->first->GetDestNetworkMask (), mainRoute, eslr::MAIN);

          FindRouteInBackup (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), primaryRoute, eslr::PRIMARY);
          
          // if the main route's cost is higher
          // and if the main route is about to expire
          // and the primary route is about to delete because it is expiring
          // update main route before the primary route is getting deleted
          if (foundInMain && 
              (Simulator::GetDelayLeft (mainRoute->second) < (param.invalidateTime/4)) && 
              (mainRoute->first->GetMetric () > bkupRoute->first->GetMetric ()) && 
              ((param.invalidateType == eslr::EXPIRE)))
          {
            RoutingTableEntry* route = new RoutingTableEntry (bkupRoute->first->GetDestNetwork (), bkupRoute->first->GetDestNetworkMask (), bkupRoute->first->GetGateway (), bkupRoute->first->GetInterface ());
            route->SetValidity (eslr::VALID);
            route->SetSequenceNo (bkupRoute->first->GetSequenceNo ());
            route->SetRouteType (eslr::PRIMARY);
            route->SetMetric (bkupRoute->first->GetMetric ());
            route->SetRouteChanged (true);

            bkupRoute->first->SetValidity (eslr::INVALID);
            bkupRoute->first->SetRouteChanged (true);
            //if (secondaryRoute->second.IsRunning ())
            bkupRoute->second.Cancel (); 
            m_backupRoutingTable.erase (bkupRoute);
            //secondaryRoute->second = Simulator::Schedule (param.deleteTime, &RoutingTable::DeleteRoute, this, secondaryRoute->first, eslr::BACKUP);

            mainRoute->second.Cancel ();
            m_mainRoutingTable.erase (mainRoute);
            primaryRoute->second.Cancel ();
            m_backupRoutingTable.erase (primaryRoute);

            Time delay = Seconds (m_rng->GetValue (0.0, 5.0));
            //UpdateNetworkRoute (route, param.invalidateTime, param.deleteTime, delay, eslr::BACKUP); 
            AddRoute (route, param.invalidateTime, param.deleteTime, delay, eslr::BACKUP);
          }
          else
          {
            bkupRoute->first->SetValidity (eslr::INVALID);
            bkupRoute->first->SetRouteChanged (true); 

            //if (bkupRoute->second.IsRunning ())
              bkupRoute->second.Cancel (); 
              
            Time delay = param.deleteTime + Seconds (m_rng->GetValue (0.0, 5.0));           
            bkupRoute->second = Simulator::Schedule (delay, &RoutingTable::DeleteRoute, this, bkupRoute->first, eslr::BACKUP);            
          }
        }
        else if (bkupRoute->first->GetRouteType () == eslr::PRIMARY)
        {
          // Primary routes are never invalidated. Thoes routes are deleted along with the Main routes.
          // even accidently does, in this point, that will be corrected.
          //if (bkupRoute->second.IsRunning ())
            bkupRoute->second.Cancel (); 
          bkupRoute->second = EventId ();  
        } 
       
        return retVal = true;
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

  // Assume that ony two routes are avilable in the backup routing table and, 
  // one is marked as PRIMARY and the other one is marked as SECONDARY
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
    bool retVal = false;

    if (table == eslr::MAIN)
    {
      RoutesI mainRoute, primaryRoute;

      bool foundInMain = FindRouteRecord (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), mainRoute, table);

      FindRouteInBackup (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), primaryRoute, eslr::PRIMARY);

      if (foundInMain && (routingTableEntry->GetGateway () != Ipv4Address::GetZero ()))
      {
        RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
        route->SetValidity (routingTableEntry->GetValidity ());
        route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        route->SetRouteType (eslr::PRIMARY);
        route->SetMetric (routingTableEntry->GetMetric ());
        route->SetRouteChanged (true);

        delete mainRoute->first;
        mainRoute->first = route;

        //if (mainRoute->second.IsRunning ())
          mainRoute->second.Cancel ();   

        invalidateParams  p;
        p.invalidateTime = invalidateTime;
        p.deleteTime = deleteTime;
        p.settlingTime = settlingTime;
        p.invalidateType = eslr::EXPIRE;
        p.table = eslr::MAIN;     

        Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 10.0));          
        mainRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, mainRoute->first, p);

        delete primaryRoute->first;
        primaryRoute->first = mainRoute->first;
        primaryRoute->first->SetRouteType (eslr::PRIMARY);
        primaryRoute->first->SetRouteChanged (true);

        //if (primaryRoute->second.IsRunning ())
          primaryRoute->second.Cancel (); 
        
        primaryRoute->second  = EventId (); 

        retVal = true;
      }
      else if (foundInMain && (routingTableEntry->GetGateway () == Ipv4Address::GetZero ()))
      {
        RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
        route->SetValidity (routingTableEntry->GetValidity ());
        route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
        route->SetRouteType (eslr::PRIMARY);
        route->SetMetric (routingTableEntry->GetMetric ());
        route->SetRouteChanged (routingTableEntry->GetRouteChanged ());

        delete mainRoute->first;
        mainRoute->first = route;

        retVal = true;
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
          RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());

          route->SetValidity (routingTableEntry->GetValidity ());
          route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
          route->SetRouteType (eslr::PRIMARY);
          route->SetMetric (routingTableEntry->GetMetric ());
          route->SetRouteChanged (true);

          delete primaryRoute->first;
          primaryRoute->first = route;

          //if (primaryRoute->second.IsRunning ())
            primaryRoute->second.Cancel ();  

          Time delay = settlingTime + Seconds (m_rng->GetValue (0.0, 5.0));          
          primaryRoute->second = Simulator::Schedule (delay, &RoutingTable::MoveToMain, this, primaryRoute->first, invalidateTime, deleteTime, settlingTime);

            retVal = true; 
        }
        else
          retVal = false;
      }
      else if (routingTableEntry->GetRouteType () == eslr::SECONDARY)
      {
        if (foundSecondary)
        {
          // if the route record is secondary record, update it and invalidate it after timeout.
          RoutingTableEntry* route = new RoutingTableEntry (routingTableEntry->GetDestNetwork (), routingTableEntry->GetDestNetworkMask (), routingTableEntry->GetGateway (), routingTableEntry->GetInterface ());
          route->SetValidity (routingTableEntry->GetValidity ());
          route->SetSequenceNo (routingTableEntry->GetSequenceNo ());
          route->SetRouteType (eslr::SECONDARY);
          route->SetMetric (routingTableEntry->GetMetric ());
          route->SetRouteChanged (true);

          delete secondaryRoute->first;
          secondaryRoute->first = route;

          //if (secondaryRoute->second.IsRunning ())
            secondaryRoute->second.Cancel (); 

          invalidateParams  p;
          p.invalidateTime = invalidateTime;
          p.deleteTime = deleteTime;
          p.settlingTime = settlingTime;
          p.invalidateType = eslr::EXPIRE;
          p.table = eslr::BACKUP;      

          Time delay = invalidateTime + Seconds (m_rng->GetValue (0.0, 10.0));          
          secondaryRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, secondaryRoute->first, p);     
            
          retVal = true;            
        }
        else 
          retVal = false;
      }
    }
    return retVal;
  }

  void 
  RoutingTable::ReturnRoutingTable (RoutingTableInstance &instance, eslr::Table table)
  {
    if (table == eslr::MAIN)
    {
      // Note: In the case of Main routing table, at the time returning the routing table, 
      // a newly created separate instance is returned. Note that, in order to manage memory properly, 
      // table instance has to be cleared at the place requesting the entire main routing table.
      
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
      *os << "Destination         Gateway          If  Seq#    Metric  Validity Changed Expire in (s)" << '\n';
      *os << "------------------  ---------------  --  ------  ------  -------- ------- -------------" << '\n';

      for (RoutesCI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
      {
        RoutingTableEntry *route = it->first;
        eslr::Validity validity = route->GetValidity ();

        if (validity == eslr::VALID || validity == eslr::LHOST || validity == eslr::INVALID)
        {
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
          *os << std::setiosflags (std::ios::left) << std::setw (10) << val.str ();
          *os << std::setiosflags (std::ios::left) << std::setw (7) << route->GetRouteChanged ();
          *os << std::setiosflags (std::ios::left) << std::setw (8) << Simulator::GetDelayLeft (it->second).GetSeconds ();
         
          *os << '\n';
        }        
      }
      //*os << "---------------------------------------------------------------------------------------" << '\n';
    }
    else if (table == eslr::BACKUP)
    {
      *os << "Destination         Gateway          If  Seq#    Metric  Validity  Pri/Sec Next Event (s)" << '\n';
      *os << "------------------  ---------------  --  ------  ------  --------  ------- --------------" << '\n';

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
        else
          validity << "INVALID";
        *os << std::setiosflags (std::ios::left) << std::setw (10) << validity.str ();
        if (route->GetRouteType () == eslr::PRIMARY)
          preSec << "P";
        else
          preSec << "S";
        *os << std::setiosflags (std::ios::left) << std::setw (8) << preSec.str ();
          *os << std::setiosflags (std::ios::left) << std::setw (8) << Simulator::GetDelayLeft (it->second).GetSeconds ();
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
    RoutingTableInstance routesInMain, routesInBackup;
    bool foundInMain, foundInBackup;
    
    foundInMain = FindRoutesForGateway (gateway, routesInMain, eslr::MAIN);
    foundInBackup = FindRoutesForGateway (gateway, routesInBackup, eslr::BACKUP);

    if (foundInMain)
    {
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

        Time delay = Seconds (invalidateTime.GetSeconds () / 10) + Seconds (m_rng->GetValue (0.0, 15.0));
        it->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, it->first, p);
      }
    }
    if (foundInBackup)
    {
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

        Time delay = Seconds (invalidateTime.GetSeconds () / 10) + Seconds (m_rng->GetValue (0.0, 15.0));
       it->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, it->first, p);
      }
    }
  }

  void 
  RoutingTable::InvalidateRoutesForInterface (uint32_t interface,  Time invalidateTime, Time deleteTime, Time settlingTime)
  {
    RoutingTableInstance routesInMain, routesInBackup;
    bool foundInMain, foundInBackup;
    
    foundInMain = FindRoutesForInterface (interface, routesInMain, eslr::MAIN);
    foundInBackup = FindRoutesForInterface (interface, routesInBackup, eslr::BACKUP);

    if (foundInMain)
    {
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

        Time delay = Seconds (invalidateTime.GetSeconds () / 10) + Seconds (m_rng->GetValue (0.0, 15.0));
        it->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, it->first, p);
      }
    }
    if (foundInBackup)
    {
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

        Time delay = Seconds (invalidateTime.GetSeconds () / 10) + Seconds (m_rng->GetValue (0.0, 15.0));
        it->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, it->first, p);
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

        Time delay = Seconds (invalidateTime.GetSeconds () / 10) + Seconds (m_rng->GetValue (0.0, 15.0));
        foundRoute->second = Simulator::Schedule (delay, &RoutingTable::InvalidateRoute, this, foundRoute->first, p); 
      retVal = true;   
    }
    return retVal;
  }
  
  void 
  RoutingTable::ToggleRouteChanged ()
  {
    for (RoutesI it = m_mainRoutingTable.begin ();  it!= m_mainRoutingTable.end (); it++)
    {
      it->first->SetRouteChanged (false);
    }
  }
  
  void 
  RoutingTable::IncrementSeqNo ()
  {
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
