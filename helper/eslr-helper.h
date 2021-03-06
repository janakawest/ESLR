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

#ifndef ESLR_HELPER_H
#define ESLR_HELPER_H

#include "ns3/eslr-main.h"

#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/object-factory.h"

namespace ns3 {
/**
 * \brief Helper class that adds ESLR routing to nodes.
 *
 * This class is expected to be used in conjunction with
 * ns3::InternetStackHelper::SetRoutingHelper
 *
 */
class EslrHelper : public Ipv4RoutingHelper
{
public:
  /*
   * Constructor.
   */
  EslrHelper (); 

  /**
   * \brief Construct an EslrHelper from previously
   * initialized instance (Copy Constructor).
   */
  EslrHelper (const EslrHelper &);

  /**
   * Ending the instence by destructing the EslrHelper
   */
  virtual ~EslrHelper ();

  /**
   * \returns pointer to clone of this EslrHelper
   *
   * This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   */
  EslrHelper* Copy (void) const;

  /**
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   *
   * This method will be called by ns3::InternetStackHelper::Install
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;


  /**
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set.
   *
   * This method controls the attributes of ns3::eslr
   */
  void Set (std::string name, const AttributeValue &value);

  /**
   * Assign a fixed random variable stream number for the random variables
   * used by this model. Return the number of streams (possibly zero) that
   * have been assigned. The Install() method should have previously been
   * called by the user.
   *
   * \param c NetDeviceContainer of the set of net devices for which the
   *          SixLowPanNetDevice should be modified to use a fixed stream
   * \param stream first stream index to use
   * \return the number of stream indices assigned by this helper
   */
  int64_t AssignStreams (NodeContainer c, int64_t stream);

  /**
   * \brief Install a default route for the node.
   *
   * The traffic will be forwarded to the nextHop, located on the specified
   * interface, unless a specific route record is found.
   *
   * \param node the node
   * \param nextHop the next hop
   * \param interface the network interface
   */
  void SetDefRoute (Ptr<Node> node, Ipv4Address nextHop, uint32_t interface);

  /**
   * \brief Exclude an interface from Eslr protocol.
   *
   * You have to call this function BEFORE installing ESLR for the nodes.
   *
   * Note: the exclusion means that ESLR route updates will not be propagated on the excluded interface.
   * The network prefix on that interface will be still considered in ESLR.
   *
   * \param node the node
   * \param interface the network interface to be excluded
   */
  void ExcludeInterface (Ptr<Node> node, uint32_t interface);

private:
  /**
   * \brief Assignment operator declared private and not implemented to disallow
   * assignment and prevent the compiler for inserting its own.
   */
  EslrHelper &operator = (const EslrHelper &o);

  ObjectFactory m_factory; //!< Object Factory

  std::map< Ptr<Node>, std::set<uint32_t> > m_interfaceExclusions; //!< Interface Exclusion set

}; // end of the EslrHelper class

}// end of namespace ns3

#endif /* ESLR_HELPER_H */

