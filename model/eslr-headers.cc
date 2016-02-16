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
#include "eslr-definition.h"
#include "eslr-headers.h"

namespace ns3 {
namespace eslr {
/*
 * ESLR RUM
*/
NS_OBJECT_ENSURE_REGISTERED (ESLRrum);

ESLRrum::ESLRrum(): m_sequenceNumber (0),
                    m_matric (0),
                    m_destination (Ipv4Address ()),
                    m_mask (Ipv4Mask ()),
										/*K1 (0),
										K2 (0),
										K3 (0),*/
										m_routeTag (0)	
{ 
	/*Constructor*/ 
}
TypeId ESLRrum::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::eslr::ESLRrum").SetParent<Header> ().AddConstructor<ESLRrum> ();
  return tid;
}

TypeId ESLRrum::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void 
ESLRrum::Print (std::ostream & os) const
{
  os << 
		"Destination " <<  m_destination << "/" << m_mask << 
		" Matric " << int (m_matric) << 
		" Tag " << int (m_routeTag) << 
		" Sequoence Number "<< int (m_sequenceNumber) << 
		/*" K1 " << (int)K1 << 
		" K2 " << (int)K2 <<
	  " K3 " << (int)K3 << */std::endl;
}

uint32_t 
ESLRrum::GetSerializedSize () const
{
  return RUM_SIZE;
}

void 
ESLRrum::Serialize (Buffer::Iterator i) const
{
  i.WriteHtonU16 (m_sequenceNumber);
	i.WriteHtonU32 (m_matric);
	//i.WriteU8 (K1);
	//i.WriteU8 (K2);
	//i.WriteU8 (K3);
	i.WriteU8 (m_routeTag);
	uint8_t tmp[4];
  m_destination.Serialize (tmp);
  i.Write (tmp, 4);
  i.WriteHtonU32 (m_mask.Get ());
}

uint32_t 
ESLRrum::Deserialize (Buffer::Iterator i)
{
	m_sequenceNumber = i.ReadNtohU16 ();
  m_matric = i.ReadNtohU32 ();
	//K1 = i.ReadU8 ();
  //K2 = i.ReadU8 ();
  //K2 = i.ReadU8 ();
  m_routeTag = i.ReadU8 ();
  uint8_t tmp[4];
  i.Read (tmp, 4);
  m_destination  = Ipv4Address::Deserialize (tmp);
  m_mask = i.ReadNtohU32 ();

  return GetSerializedSize ();
}

std::ostream & operator << (std::ostream & os, const ESLRrum & RUM)
{
  RUM.Print (os);
  return os;
}

/*
* KAM Header
*/

NS_OBJECT_ENSURE_REGISTERED (KAMHeader);

KAMHeader::KAMHeader(): m_command (0),
                        m_authType (0),
                        m_authData (0),
												m_identifier (0),
                        m_neighborID (0),
                        m_gateway (Ipv4Address ()),
                        m_gatewayMask (Ipv4Mask ())
{ /*Constructor*/ }

TypeId KAMHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::eslr::KAMHeader").SetParent<Header> ().AddConstructor<KAMHeader> ();
  return tid;
}

TypeId KAMHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void 
KAMHeader::Print (std::ostream & os) const
{
  os << "ID " << m_neighborID << " Destination " <<  m_gateway << "/" << m_gatewayMask << " Authentication Type " << int (m_authType) << std::endl;
}

uint32_t 
KAMHeader::GetSerializedSize () const
{
  return KAM_SIZE;
}

void 
KAMHeader::Serialize (Buffer::Iterator i) const
{
 	i.WriteU8 (m_command);
  i.WriteU8 (m_authType);
  i.WriteHtonU16 (m_authData);
	i.WriteU8 (m_identifier);
  i.WriteHtonU16 (m_neighborID);
  uint8_t tmp[4];
  m_gateway.Serialize (tmp);
  i.Write (tmp, 4);
  i.WriteHtonU32 (m_gatewayMask.Get ());
}

uint32_t 
KAMHeader::Deserialize (Buffer::Iterator i)
{
  m_command = i.ReadU8 ();
  m_authType = i.ReadU8 ();
  m_authData = i.ReadNtohU16 ();
	m_identifier = i.ReadU8 ();
	m_neighborID = i.ReadNtohU16 ();
  uint8_t tmp[4];
  i.Read (tmp, 4);
  m_gateway  = Ipv4Address::Deserialize (tmp);
  m_gatewayMask = i.ReadNtohU32 ();

  return GetSerializedSize ();
}

std::ostream & operator << (std::ostream & os, const KAMHeader & KAM)
{
  KAM.Print (os);
  return os;
}

/*
* SRC Header
*/

NS_OBJECT_ENSURE_REGISTERED (SRCHeader);

SRCHeader::SRCHeader(): m_seqNo (0),
												m_flagSet (0),
												m_mue (0),
                        m_lambda (0),
                        m_serverAddress (Ipv4Address ()),
                        m_netMask (Ipv4Mask ())
{ /*Constructor*/ }

TypeId SRCHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::eslr::SRCHeader").SetParent<Header> ().AddConstructor<SRCHeader> ();
  return tid;
}

TypeId SRCHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void 
SRCHeader::Print (std::ostream & os) const
{
  os << "Server Address " <<  m_serverAddress << "/" << m_netMask << " Lambda " << int (m_lambda)  << " Mue "<< int (m_mue) << std::endl;
}

uint32_t 
SRCHeader::GetSerializedSize () const
{
  return SRCH_SIZE;
}

void 
SRCHeader::Serialize (Buffer::Iterator i) const
{
 	i.WriteHtonU16 (m_seqNo);
  i.WriteHtonU16 (m_flagSet);
  i.WriteHtonU32 (m_mue);
  i.WriteHtonU32 (m_lambda);
	uint8_t tmp[4];
  m_serverAddress.Serialize (tmp);
  i.Write (tmp, 4);
  i.WriteHtonU32 (m_netMask.Get ());
}

uint32_t 
SRCHeader::Deserialize (Buffer::Iterator i)
{
	m_seqNo = i.ReadNtohU16 ();	
	m_flagSet = i.ReadNtohU16 ();
  m_mue = i.ReadNtohU32 ();
  m_lambda = i.ReadNtohU32 ();
  uint8_t tmp[4];
  i.Read (tmp, 4);
  m_serverAddress  = Ipv4Address::Deserialize (tmp);
  m_netMask = i.ReadNtohU32 ();

  return GetSerializedSize ();
}

std::ostream & operator << (std::ostream & os, const SRCHeader & SRC)
{
  SRC.Print (os);
  return os;
}

/*
* ESLR Routing Header
*/

NS_OBJECT_ENSURE_REGISTERED (ESLRRoutingHeader);

ESLRRoutingHeader::ESLRRoutingHeader () : m_command (0),
                                          m_ruType (0),
                                          m_reqType (0),
                                          m_noe (0),
                                          m_authType (0),
                                          m_authData (0),
																					m_advertisementType (0)
{ /*Constructor*/ }

TypeId ESLRRoutingHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::eslr::ESLRRoutingHeader").SetParent<Header> ().AddConstructor<ESLRRoutingHeader> ();
  return tid;
}

TypeId ESLRRoutingHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void 
ESLRRoutingHeader::Print (std::ostream & os) const
{
  os << "Command " << int (m_command);
  os << " Route Update Type " << int (m_ruType);
  os << " Route Update Request Type  " << int (m_reqType);
  if (m_reqType == NE)
    os << " Requested Number of Entries " << int(m_noe);
  os << " Authentication Type " << int(m_authType);
  os << " Aunthentication Data " << int(m_authData);
	os << " Route Update Type " << int(m_advertisementType);
	if (GetFastTrigUpdate ())
		os << " A Fast Triggered Update ";
	else if (GetPeriodicUpdate())
		os << " A Periodic Update ";
	else if (GetTrigUpdate ())
		os << " A Regular Triggered Update ";
	if (GetCbit ())
		os << " The routes are possibally connected routes ";
	else if (GetDbit ())
		os << " The routes are possibally poisoned routes "; 
  if (m_command == RU)
  {
    for (std::list<ESLRrum>::const_iterator iter = m_rumList.begin (); iter != m_rumList.end (); iter ++)
    {
      os << " RUMS |  ";
      iter->Print (os);
    }
  }
  else if (m_command == KAM)
  {
    for (std::list<KAMHeader>::const_iterator iter = m_helloList.begin (); iter != m_helloList.end (); iter ++)
    {
      os << " KAMS |  ";
      iter->Print (os);
    }
  }
  else if (m_command == SRC)
  {
    for (std::list<SRCHeader>::const_iterator iter = m_serverList.begin (); iter != m_serverList.end (); iter ++)
    {
      os << " SCR |  ";
      iter->Print (os);
    }
  }
}

uint32_t 
ESLRRoutingHeader::GetSerializedSize () const
{
  ESLRrum rum;
  KAMHeader kam;
  SRCHeader src;
  if (m_command == RU) // Get the size of the Route Update packet
  {
    return ESLR_BASE_SIZE + m_rumList.size () * rum.GetSerializedSize ();
  }
  else if (m_command == KAM) // Get the size of the Hello/Keep Alive Message packet
  {
    return ESLR_BASE_SIZE + m_helloList.size () * kam.GetSerializedSize ();
  }
  else if (m_command == SRC) // Get the size of the Server-Router Communication packet
  {
    return ESLR_BASE_SIZE + m_serverList.size () * src.GetSerializedSize ();
  }
  return 0; // return 0 if the message miss matches. 
}

void 
ESLRRoutingHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  i.WriteU8 (m_command);
  i.WriteU8 (m_ruType);
  i.WriteU8 (m_reqType);
  i.WriteU8 (m_noe);
  i.WriteU8 (m_advertisementType);
  i.WriteU8 (m_authType);
  i.WriteHtonU16 (m_authData);

  if (m_command == RU) // Get the size of the Route Update packet
  {
    for (std::list<ESLRrum>::const_iterator iter = m_rumList.begin (); iter != m_rumList.end (); iter ++)
    {
      iter->Serialize (i);
      i.Next(iter->GetSerializedSize ());
    }
  }
  else if (m_command == KAM) // Get the size of the Hello/Keep Alive Message packet
  {
    for (std::list<KAMHeader>::const_iterator iter = m_helloList.begin (); iter != m_helloList.end (); iter ++)
    {
      iter->Serialize (i);
      i.Next(iter->GetSerializedSize ());
    }
  }
  else if (m_command == SRC) // Get the size of the Server-Router Communication packet
  {
    for (std::list<SRCHeader>::const_iterator iter = m_serverList.begin (); iter != m_serverList.end (); iter ++)
    {
      iter->Serialize (i);
      i.Next(iter->GetSerializedSize ());
    }
  }
}

uint32_t 
ESLRRoutingHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  uint8_t temp;
  temp = i.ReadU8 ();
  if ((temp == RU) || (temp == KAM) || (temp == SRC))
  {
    m_command = temp;
  }
  else
  {
    // Return 0 to indicate that the received packet is not in order.
    return 0;
  }

  m_ruType = i.ReadU8 ();
  m_reqType = i.ReadU8 ();
  m_noe = i.ReadU8 ();
  m_advertisementType = i.ReadU8 ();
  m_authType = i.ReadU8 ();
  m_authData = i.ReadNtohU16 ();

  uint8_t numberofMessages = 0;

  if (m_command == RU) // Get the size of the Route Update packet
  {
    numberofMessages = (i.GetSize () - ESLR_BASE_SIZE) / RUM_SIZE;
    for (uint8_t n=0; n<numberofMessages; n++)
    {
      ESLRrum rum;
      i.Next (rum.Deserialize (i));
      m_rumList.push_back (rum);
    }
    return GetSerializedSize ();
  }
  else if (m_command == KAM) // Get the size of the Hello/Keep Alive Message packet
  {
    numberofMessages = (i.GetSize () - ESLR_BASE_SIZE) / KAM_SIZE;
    for (uint8_t n=0; n<numberofMessages; n++)
    {
      KAMHeader kam;
      i.Next (kam.Deserialize (i));
      m_helloList.push_back (kam);
    }
    return GetSerializedSize ();
  }
  else if (m_command == SRC) // Get the size of the Server-Router Communication packet
  {
    numberofMessages = (i.GetSize () - ESLR_BASE_SIZE) / SRCH_SIZE;
    for (uint8_t n=0; n<numberofMessages; n++)
    {
      SRCHeader src;
      i.Next (src.Deserialize (i));
      m_serverList.push_back (src);
    }
    return GetSerializedSize ();
  }
  else
  {
    // Return 0 to indicate that the received packet is not in order.
    return 0;
  }
}

void 
ESLRRoutingHeader::AddRum (ESLRrum rum)
{
  m_rumList.push_back (rum);
  SetNoe ();
}

void 
ESLRRoutingHeader::DeleteRum (ESLRrum rum)
{
  for (std::list<ESLRrum>::iterator it = m_rumList.begin (); it!= m_rumList.end (); it ++)
  {
    if (rum.GetDestAddress () == it->GetDestAddress ())
    { 
      m_rumList.erase (it);
    }
  }    
}

void 
ESLRRoutingHeader::ClearRums ()
{
  m_rumList.clear ();
}

std::list<ESLRrum> 
ESLRRoutingHeader::GetRumList (void) const
{
  return m_rumList;
}

void 
ESLRRoutingHeader::AddKam (KAMHeader kam)
{
  m_helloList.push_back (kam);
}

void 
ESLRRoutingHeader::ClearKams ()
{
  m_helloList.clear ();
}

std::list<KAMHeader> 
ESLRRoutingHeader::GetKamList (void) const
{
  return m_helloList;
}

void 
ESLRRoutingHeader::AddSrc (SRCHeader src)
{
  m_serverList.push_back (src);
}

void 
ESLRRoutingHeader::ClearSrcs ()
{
  m_serverList.clear ();
}

std::list<SRCHeader> 
ESLRRoutingHeader::GetSrcList (void) const
{
  return m_serverList;
}

std::ostream & operator << (std::ostream & os, const ESLRRoutingHeader & h)
{
  h.Print (os);
  return os;
}
} // end of eslr namespace
} // end of ns3 namespace
