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

#ifndef ESLR_DEFINITION_HEADER
#define ESLR_DEFINITION_HEADER

namespace ns3 {
namespace eslr {

/**
 * Header sizes of the ESLR Protocol
 */

  #define RUM_SIZE 17 //!< Route Update Message (RUM) Size (No CCVs)
  #define KAM_SIZE 15 //!< Keep-alive Message (KAM) Size
  #define SRCH_SIZE 20 //!< Server-router Communication (SRC) Size
  #define ESLR_BASE_SIZE 8 //!< ESLR Header Base Size

/**
 * Authentication types used in route management
 */
  enum AuthType {
    PLAIN_TEXT = 0x01, //!< Auth data send as PLAIN-TEXT (Current Implementation support only plain text)
    MD5 = 0x02, //!< Auth data send as MD5 HASH
    SHA = 0x03, //!< Auth data send as SHA HASH
  };

/**
 * Commands used in KAM Header
 */
  enum KamHeaderCommand
  {
    HELLO = 0x01,
		HI = 0x02,
  };

/**
 * Commands used in ESLR Routing Header
 */
  enum EslrHeaderCommand
  {
    RU = 0x01, //!< Route Update message
    KAM = 0x03, //!< Hello, Hi, Keep alive messages
    SRC = 0x04, //!< Server-router communication message
  };

/**
 * Commands used in Route Update Type (RU-Type)
 */
  enum EslrHeaderRUCommand
  {
    NO = 0x00, //!< No. This is set when ESLR header carries KAM and SRC messages
    REQUEST = 0x01, //!< ESLR request message
    RESPONSE = 0x02, //!< ESLR response message
  };

/**
 * Route Request Types
 */
  enum EslrHeaderRequestType
  {
    NON = 0x00, //!< No request. This is set when ESLR header used to send KAM and SRC messages
    OE = 0x01, //!< One Entry 
    NE = 0x02, //!< Number of Entries specified in the ESLRRoutingHeader::NoE
    ET = 0x03, //!< Entire Table
		ND = 0x04, //!< Specially maintained to initial route discovery.
							 //   Those who send initial route request, should send this and the packet
							 //   will be treated as ET
    RESPOND = 0xff, //!< For all response messages
  };

/**
 * Validity types of both neighbor and route records 
 */
enum Validity
{
  INVALID = 0x00, //!< Invalid Neighbor or Route Record
  VALID = 0x01, //!< Valid Neighbor or Route Record
  DISCONNECTED = 0x02, //!< Disconnected Route Record
  LHOST = 0x03, //!< Host route for the loop-back interface. This is not used for route advertisements
	VOID = 0x04, //!< The initial state that the neighbors are added in  
};

/**
  * Indicate whether the route record is presented in Main Table and Backup Table 
  */
enum RouteType
{
  PRIMARY = 0x01, //!< routes in main table and the reference routers in in B-Table
  SECONDARY = 0x02, //!< Backup routes in B-Table
};

/**
  * The Main Table and Backup Table 
  */
enum Table
{
  MAIN = 0x01, //!< main table: M-Table
  BACKUP = 0x02, //!< backup table: B-Table
};

/**
  * Update type 
  */
enum UpdateType
{
  PERIODIC = 0x01, //!< ESLR Periodic Update
  TRIGGERED = 0x02, //!< ESLR Triggered Update (Fast triggered updates are differentiated using ESLRHeader structure)
};

/**
  * Invalidation type
  */
enum InvalidateType
{
  EXPIRE = 0x01, //!< Invalidate a route due to expiration
  BROKEN = 0x02, //!< Invalidate a route due to broken destination 
  BROKEN_NEIGHBOR = 0x03, //!< Invalidate a route due to unresponsive neighbor
  BROKEN_INTERFACE = 0x04, //!< Invalidate a route due to unresponsive interface
};

/**
 * Split Horizon strategy type.
 */
enum SplitHorizonType {
  NO_SPLIT_HORIZON, //!< No Split Horizon
  SPLIT_HORIZON,   //!< Split Horizon
};

/**
 * Printing Options.
 */
enum PrintingOption {
  DONT_PRINT, //!< Do not print any table (Default state)
  MAIN_R_TABLE, //!< Print the main routing table
  BACKUP_R_TABLE, //!< Print the backup routing table
  N_TABLE, //!< Print the neighbor table
};

} // end of eslr namespace
} // end of ns3 namespace
#endif /* DEFINITION */
