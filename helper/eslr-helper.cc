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

#include "eslr-helper.h"

#include "ns3/eslr-main.h"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"

namespace ns3 {

  EslrHelper::EslrHelper () : Ipv4RoutingHelper ()
                              
  {
    m_factory.SetTypeId ("ns3::eslr::EslrRoutingProtocol");
  }

  EslrHelper::EslrHelper (const EslrHelper &o): m_factory (o.m_factory)
  {
    m_interfaceExclusions = o.m_interfaceExclusions;
  }

  EslrHelper::~EslrHelper ()
  {
    m_interfaceExclusions.clear ();
  }

  EslrHelper* 
  EslrHelper::Copy (void) const
  {
        return new EslrHelper (*this);
  }

  Ptr<Ipv4RoutingProtocol>
  EslrHelper::Create (Ptr<Node> node) const
  {
    Ptr<eslr::EslrRoutingProtocol> eslrRouteProto = m_factory.Create<eslr::EslrRoutingProtocol> ();

    std::map<Ptr<Node>, std::set<uint32_t> >::const_iterator it = m_interfaceExclusions.find (node);

    if(it != m_interfaceExclusions.end ())
    {
      eslrRouteProto->SetInterfaceExclusions (it->second);
    }

    node->AggregateObject (eslrRouteProto);
    return eslrRouteProto;
  }

  void
  EslrHelper::Set (std::string name, const AttributeValue &value)
  {
    m_factory.Set (name, value);
  } 

  int64_t
  EslrHelper::AssignStreams (NodeContainer c, int64_t stream)
  {
    int64_t currentStream = stream;
    Ptr<Node> node;
    for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      node = (*i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      NS_ASSERT_MSG (ipv4, "Ipv4 not installed on node");
      Ptr<Ipv4RoutingProtocol> rProto = ipv4->GetRoutingProtocol ();
      NS_ASSERT_MSG (rProto, "Ipv4 routing not installed on node");
      Ptr<eslr::EslrRoutingProtocol> Eslr = DynamicCast<eslr::EslrRoutingProtocol> (rProto);
      if (Eslr)
      {
        currentStream += Eslr->AssignStreams (currentStream);
        continue;
      }
      // ESLR may also be in a list
      Ptr<Ipv4ListRouting> routeList = DynamicCast<Ipv4ListRouting> (rProto);
      if (routeList)
      {
        int16_t priority;
        Ptr<Ipv4RoutingProtocol> listIpv4Proto;
        Ptr<eslr::EslrRoutingProtocol> listEslr;
        for (uint32_t i = 0; i < routeList->GetNRoutingProtocols (); i++)
        {
          listIpv4Proto = routeList->GetRoutingProtocol (i, priority);
          listEslr = DynamicCast<eslr::EslrRoutingProtocol> (listIpv4Proto);
          if (listEslr)
          {
            currentStream += Eslr->AssignStreams (currentStream);
            break;
          }
        }
      }
    }
    return (currentStream - stream);
  }
  
  void EslrHelper::SetDefRoute (Ptr<Node> node, Ipv4Address nextHop, uint32_t interface)
  {
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    NS_ASSERT_MSG (ipv4, "Ipv4 not installed on node");
    Ptr<Ipv4RoutingProtocol> rProto = ipv4->GetRoutingProtocol ();
    NS_ASSERT_MSG (rProto, "Ipv4 routing not installed on node");
    Ptr<eslr::EslrRoutingProtocol> Eslr = DynamicCast<eslr::EslrRoutingProtocol> (rProto);
    if (Eslr)
    {
      // To do: Implement the Default route method in the eslr-main
      //Eslr->AddDefaultRouteTo (nextHop, interface);
    }
      // ESLR may also be in a list
    Ptr<Ipv4ListRouting> routeList = DynamicCast<Ipv4ListRouting> (rProto);
    if (routeList)
    {
      int16_t priority;
      Ptr<Ipv4RoutingProtocol> listIpv4Proto;
      Ptr<eslr::EslrRoutingProtocol> listEslr;
      for (uint32_t i = 0; i < routeList->GetNRoutingProtocols (); i++)
      {
        listIpv4Proto = routeList->GetRoutingProtocol (i, priority);
        listEslr = DynamicCast<eslr::EslrRoutingProtocol> (listIpv4Proto);
        if (listEslr)
        {
          // To do: Implement the Default route method in the eslr-main
          //Eslr->AddDefaultRouteTo (nextHop, interface);
          break;
        }
      }
    }
  }

  void
  EslrHelper::ExcludeInterface (Ptr<Node> node, uint32_t interface)
  {
    std::map< Ptr<Node>, std::set<uint32_t> >::iterator it = m_interfaceExclusions.find (node);

    if (it == m_interfaceExclusions.end ())
      {
        std::set<uint32_t> interfaces;
        interfaces.insert (interface);

        m_interfaceExclusions.insert (std::make_pair (node, interfaces));
      }
    else
      {
        it->second.insert (interface);
      }
  }

}// end of namespave ns3

