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

#ifndef ESLR_PACKET_HEADER
#define ESLR_PACKET_HEADER

#include <list>

#include "eslr-definition.h"

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"

namespace ns3 {
namespace eslr {

/**
 * \ingroup ESLR
 * \brief ESLR Route Update Message (RUM)
 */
/**	-----------------------------RUM-------------------------------
	* |      0        |      1        |      2        |      3      |
	* 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |				Seq#		                |			     Metric	            |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |    Route tag				          |   Prefix Len  |      NA     |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |				            Network Address / Host Address		  		  |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |						               Network Mask		          				  |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
class ESLRrum : public Header
{
public:
  ESLRrum (void);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Return the instance type identifier.
   * \return instance type ID
   */
  virtual TypeId GetInstanceTypeId (void) const;

  virtual void Print (std::ostream& os) const;

  /**
   * \brief Get the serialized size of the packet.
   * \return size
   */
  virtual uint32_t GetSerializedSize (void) const;

  /**
   * \brief Serialize the packet.
   * \param start Buffer iterator
   */
  virtual void Serialize (Buffer::Iterator start) const;

  /**
   * \brief Deserialize the packet.
   * \param start Buffer iterator
   * \return size of the packet
   */
  virtual uint32_t Deserialize (Buffer::Iterator start);

  /**
   * \brief Get and Set the Sequence number.
   * \param SequenceNumber
   * \return the sequenceNumber
   */
  uint16_t GetSequenceNo () const
  {
    return m_sequenceNumber;
  }
  void SetSequenceNo (uint16_t sequenceNumber)
  {
    m_sequenceNumber = sequenceNumber;
  }

  /**
   * \brief Get and Set the Metric.
   * \param metric value
   * \return the metric value
   */
  uint16_t GetMatric () const
  {
    return m_matric;
  }
  void SetMatric (uint16_t matric)
  {
    m_matric = matric;
  }

  /**
   * \brief Get and Set the route tag.
   * \param routeTag tag value
   * \return the route tag value
   */
  uint16_t GetrouteTag () const
  {
    return m_routeTag;
  }
  void SetrouteTag (uint16_t routeTag)
  {
    m_routeTag = routeTag;
  }

  /**
   * \brief Get and Set the Destination address.
   * \param address the destination address
   * \return the destination address
   */
	void SetDestAddress (Ipv4Address destination)
	{
    m_destination = destination;
	}
	Ipv4Address GetDestAddress () const
	{
    return m_destination;
	}

  /**
   * \brief Get and Set the Network Mask
   * \param mask the netmask of the destiunation
   * \return the the netmask of the destiunation
   */
	void SetDestMask (Ipv4Mask mask)
	{
    m_mask = mask;
    m_prefixLength = m_mask.GetPrefixLength ();
	}
	Ipv4Mask GetDestMask () const
	{
    return m_mask;
	}

 /**
   * \brief Get the prefix length of the mask.
   * \return the route tag value
   */
  uint8_t GetPrefixLength () const
  {
    return m_prefixLength;
  }


private:
  uint16_t m_sequenceNumber; //!< sequence number
  uint16_t m_matric; //!< metric (time to reach the destination)
  uint16_t m_routeTag; //!< route tag
  uint8_t m_prefixLength; //!< mask length
  Ipv4Address m_destination; //!< destination network/host address
  Ipv4Mask m_mask; //!< destination network/host mask
  
};// end of class ESLRRouteUpdateMessage
/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param h the Routing Table Entry
 * \returns the reference to the output stream
 */
std::ostream & operator << (std::ostream & os, const ESLRrum & h);

/**
 * \ingroup ESLR
 * \brief ESLR Hello and Keep Alive Message (KAM) header
 */
/**	----------------Keep Alive Message Header----------------------
	* |      0        |      1        |      2        |      3      |
	* 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |	Command		    |	Auth Type	    |			    Auth DAta     		  |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |				                   Neighbor ID        							  |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |						                Gateway 	          						  |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |						                  Mask	  		        					  |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
class KAMHeader : public Header
{
public:

  KAMHeader (void);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Return the instance type identifier.
   * \return instance type ID
   */
  virtual TypeId GetInstanceTypeId (void) const;

  virtual void Print (std::ostream& os) const;

  /**
   * \brief Get the serialized size of the packet.
   * \return size
   */
  virtual uint32_t GetSerializedSize (void) const;

  /**
   * \brief Serialize the packet.
   * \param start Buffer iterator
   */
  virtual void Serialize (Buffer::Iterator start) const;

  /**
   * \brief Deserialize the packet.
   * \param start Buffer iterator
   * \return size of the packet
   */
  virtual uint32_t Deserialize (Buffer::Iterator start);

  /**
   * \brief Get and Set the Command.
   * \param command the command type
   * \return the command type
   */
	void SetCommand (eslr::KamHeaderCommand command)
	{
    m_command = command;
	}
	eslr::KamHeaderCommand Getcommand () const
	{
    return eslr::KamHeaderCommand (m_command);
	}

  /**
   * \brief Get and Set Authentication Type.
   * \param authType the authentication type
   * \return the authentication type
   */
	void SetAuthType (eslr::AuthType authType)
	{
    m_authType = authType;
	}
	eslr::AuthType GetAuthType () const
	{
    return eslr::AuthType (m_authType);
	}

  /**
   * \brief Get and Set Authentication Data.
   * \param authData the authentication Data
   * \return the authentication Data
   */
	void SetAuthData (uint16_t authData)
	{
    m_authData = authData;
	}
	uint16_t GetAuthData () const
	{
    return m_authData;
	}

  /**
   * \brief Get and Set Neghbor's ID'.
   * \param neighborID the nieghbor's ID
   * \return the nieghbor's ID
   */
	void SetNeighborID (uint32_t neighborID)
	{
    m_neighborID = neighborID;
	}
	uint32_t GetNeighborID () const
	{
    return m_neighborID;
	}

  /**
   * \brief Get and Set the Gateway address.
   * \param address the destination address
   * \return the destination address
   */
	void SetGateway (Ipv4Address gateway)
	{
    m_gateway = gateway;
	}
	Ipv4Address GetGateway () const
	{
    return m_gateway;
	}

  /**
   * \brief Get and Set the mask of the Gateway
   * \param mask the netmask of the destiunation
   * \return the the netmask of the destiunation
   */
	void SetGatewayMask (Ipv4Mask gatewayMask)
	{
    m_gatewayMask = gatewayMask;
	}
	Ipv4Mask GetGatewayMask () const
	{
    return m_gatewayMask;
	}

private:
  uint8_t m_command;  //!< message command
  uint8_t m_authType;  //!< authentication type
  uint16_t m_authData;  //!< authentication data
  uint32_t m_neighborID;  //!< neighbor's ID
  Ipv4Address m_gateway;  //!< neighbor's address
  Ipv4Mask m_gatewayMask;  //!< nieghbor's network mask
};// end of class KeepAliveMessageHeader
/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param h the Routing Table Entry
 * \returns the reference to the output stream
 */
std::ostream & operator << (std::ostream & os, const KAMHeader & h);

/**
 * \ingroup ESLR
 * \brief ESLR uses Server Router Communication (SRC) header for Server Router Communication protcol
 */
/**	--------------server-router communication header---------------
	* |      0        |      1        |      2        |      3      |
	* 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |				Seq#			              |			       Rho	    		    |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |               Mue			        |             Lambda          |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |				                   Server Address         					  |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |						              Network Mask		          				  |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
class SRCHeader : public Header
{
public:
  SRCHeader (void);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Return the instance type identifier.
   * \return instance type ID
   */
  virtual TypeId GetInstanceTypeId (void) const;

  virtual void Print (std::ostream& os) const;

  /**
   * \brief Get the serialized size of the packet.
   * \return size
   */
  virtual uint32_t GetSerializedSize (void) const;

  /**
   * \brief Serialize the packet.
   * \param start Buffer iterator
   */
  virtual void Serialize (Buffer::Iterator start) const;

  /**
   * \brief Deserialize the packet.
   * \param start Buffer iterator
   * \return size of the packet
   */
  virtual uint32_t Deserialize (Buffer::Iterator start);

  /**
   * \brief Get and Set Sequence Number.
   * \param sequenceNumber is the sequence number
   * \return the sequence number
   */
	void SetSequenceNumber (uint16_t sequenceNumber)
	{
    m_sequenceNumber = sequenceNumber;
	}
	uint16_t GetSequenceNumber () const
	{
    return m_sequenceNumber;
	}

  /**
   * \brief Get and Set Rho value.
   * \param rho is the rho value
   * \return the rho value
   */
	void SetRho (uint16_t rho)
	{
    m_rho = rho;
	}
	uint16_t GetRho () const
	{
    return m_rho;
	}

  /**
   * \brief Get and Set Mue value.
   * \param mue is the rho value
   * \return the mue value
   */
	void SetMue (uint16_t mue)
	{
    m_mue = mue;
	}
	uint16_t GetMue () const
	{
    return m_mue;
	}

  /**
   * \brief Get and Set Lambda value.
   * \param lambda is the rho value
   * \return the mue lambda
   */
	void SetLambda (uint16_t lambda)
	{
    m_lambda = lambda;
	}
	uint16_t GetLambda () const
	{
    return m_lambda;
	}

  /**
   * \brief Get and Set the server's address.
   * \param serverAddress the server's address
   * \return the server's address
   */
	void SetServerAddress (Ipv4Address serverAddress)
	{
    m_serverAddress = serverAddress;
	}
	Ipv4Address GetserverAddress () const
	{
    return m_serverAddress;
	}

  /**
   * \brief Get and Set the mask of the server
   * \param netMask the netmask of the server
   * \return the the netmask of the server
   */
	void SetNetMask (Ipv4Mask netMask)
	{
    m_netMask = netMask;
	}
	Ipv4Mask GetNetMask () const
	{
    return m_netMask;
	}

private:
  uint16_t m_sequenceNumber;  //!< sequence number
  uint16_t m_rho;  //!< Rho value (the server's utilization)
  uint16_t m_mue;  //!< Mue value (the server's average service rate)
  uint16_t m_lambda;  //!< Lambda value (the server's average packet arrival rate)
  Ipv4Address m_serverAddress;  //!< server address
  Ipv4Mask m_netMask;  //!< net mask
};// end of class ServerRouteComHeader
/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param h the Routing Table Entry
 * \returns the reference to the output stream
 */
std::ostream & operator << (std::ostream & os, const SRCHeader & h);

/**
 * \ingroup ESLR
 * \brief ESLR Routing Header
 */
/**	-------------------------ESLR header---------------------------
	* |      0        |      1        |      2        |      3      |
	* 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |    Command    |   RU. Type    |   REQ. Type   |    NoE      |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |	Auth Type	    |		      NA		|			     Auth DAta          |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	* |                            RUM ~                            |
	* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
class ESLRRoutingHeader : public Header
{
public:

  ESLRRoutingHeader (void);

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Return the instance type identifier.
   * \return instance type ID
   */
  virtual TypeId GetInstanceTypeId (void) const;

  virtual void Print (std::ostream& os) const;

  /**
   * \brief Get the serialized size of the packet.
   * \return size
   */
  virtual uint32_t GetSerializedSize (void) const;

  /**
   * \brief Serialize the packet.
   * \param start Buffer iterator
   */
  virtual void Serialize (Buffer::Iterator start) const;

  /**
   * \brief Deserialize the packet.
   * \param start Buffer iterator
   * \return size of the packet
   */
  virtual uint32_t Deserialize (Buffer::Iterator start); 

  /**
   * \brief Get and Set the Command.
   * \param command is the ESLR packet command
   * \return the command
   */
  eslr::EslrHeaderCommand GetCommand () const
  {
    return  eslr::EslrHeaderCommand (m_command);
  }
  void SetCommand (eslr::EslrHeaderCommand command)
  {
    m_command = command;
  }

  /**
   * \brief Get and Set the Route Update Message Type.
   * \param srrtr advertisement type
   * \return the advertisemetn type
   */
  eslr::EslrHeaderRUCommand GetRuCommand () const
  {
    return eslr::EslrHeaderRUCommand (m_ruType);
  }
    void SetRuCommand (eslr::EslrHeaderRUCommand ruType)
  {
    m_ruType = ruType;
  }

  /**
   * \brief Get and Set the Routing Table Request Type.
   * \param reqType represent the packet reqeuting type
   * \return the request type
   */
  eslr::EslrHeaderRequestType GetRoutingTableRequestType () const
  {
    return eslr::EslrHeaderRequestType (m_reqType);
  }
  void SetRoutingTableRequestType (eslr::EslrHeaderRequestType reqType)
  {
    m_reqType = reqType;
  }

  /**
   * \brief Get the number of RUMs included in the message
   * \brief Set method directly get the number of RUMs in the RUM list
   * \returns the number of RUMs in the message
   */
  uint8_t GetNoe () const
  {
    // Todo: change this.
    return m_noe;
  }
  void SetNoe (void)
  {
    m_noe = m_rumList.size();
  }

  /**
   * \brief Get and set the authentication type in route message exchange
   * \param the authentication type
   * \returns the authentication type
   */
  eslr::AuthType GetAuthType () const
  {
    return eslr::AuthType (m_authType);
  }
  void SetAuthType (eslr::AuthType authType)
  {
    m_authType = authType;
  }

  /**
   * \brief Get and set the authentication data
   * \param the authentication type
   * \returns the authentication type
   */
  uint16_t GetAuthData () const
  {
    return m_authData;
  }
  void SetAuthData (uint16_t authData)
  {
    m_authData = authData;
  }

  /**
   * \brief Add a RUM to the message
   * \param rum the RUM
   */
  void AddRum (ESLRrum rum);

  /**
   * \brief Delete a RUM in the message
   * \param rum the RUM
   */
  void DeleteRum (ESLRrum rum);

  /**
   * \brief Clear all the RUMs from the header
   */
  void ClearRums ();

  /**
   * \brief Get the list of the RUMs included in the message
   * \returns the list of the RUMs in the message
   */
  std::list<ESLRrum> GetRumList (void) const;

  /**
   * \brief Add a KAMs to the message
   * \param kam the keep alive message
   */
  void AddKam (KAMHeader kam);

  /**
   * \brief Clear all the KAMs from the header
   */
  void ClearKams ();

  /**
   * \brief Get the list of the KAMs included in the message
   * \returns the list of the KAMs in the message
   */
  std::list<KAMHeader> GetKamList (void) const;

  /**
   * \brief Add a SRCs to the message
   * \param src the server records
   */
  void AddSrc (SRCHeader src);

  /**
   * \brief Clear all the SRCs from the header
   */
  void ClearSrcs ();

  /**
   * \brief Get the list of the SRCs included in the message
   * \returns the list of the SRC in the message
   */
  std::list<SRCHeader> GetSrcList (void) const;

private:
  uint8_t m_command; //!< command type
  uint8_t m_ruType; //!< advertisement type
  uint8_t m_reqType; //!< request type
  uint8_t m_noe;     //!< number of RUMs are in the message
  uint8_t m_authType;     //!< Authentication Type
  uint16_t m_authData;     //!< number of RUMs are in the message

  std::list<ESLRrum> m_rumList; //!< list of the RUMs in the message
  std::list<KAMHeader> m_helloList; //!< list of the RUMs in the message
  std::list<SRCHeader> m_serverList; //!< list of the RUMs in the message

};// end of class ESLRRoutingHeader
/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param h the RIPng header
 * \returns the reference to the output stream
 */
std::ostream & operator << (std::ostream & os, const ESLRRoutingHeader & h);
} // end of eslr namespace
} // end of ns3 namespace
#endif /* HEADERS */
