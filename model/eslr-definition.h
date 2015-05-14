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

  #define RUM_SIZE 15 //!< Route Update Message Size
  #define KAM_SIZE 16 //!< Keel Alive Message Header Size
  #define SRC_SIZE 16 //!< Server Route Communication Message Header Size
  #define ESLR_BASE_SIZE 7 //!< ESLR Header Base Size

/**
 * Authentication types that are used in route management
 */
  enum AuthType {
    PLAIN_TEXT = 0x01, //!< AUTHENTICATION DATA IS SEND as PLAINTEXT
    MD5 = 0x02, //!< AUTHENTICATION DATA USED as MD5 HASH
    SHA = 0x03, //!< AUTHENTICATION DATA USED as SHA HASH
  };

/**
 * Commands to be used in KAM Header
 */
  enum KamHeaderCommand
  {
    HELLO = 0x01,
    // Rest of the commands are TBI
  };

/**
 * Commands to be used in ESLR Routing Header
 */
  enum EslrHeaderCommand
  {
    RU = 0x01, //!< Route Update message
    KAM = 0x03, //!< Keep alive (hello) message
    SRC = 0x04, //!< Server-router communication message
  };

/**
 * Commands to be used in Route Update Type (RU-Type)
 */
  enum EslrHeaderRUCommand
  {
    NO = 0x00, //!< not specified. This is set when ESLR header carries KAM and SRC messages
    REQUEST = 0x01, //!< ESLR request message
    RESPONSE = 0x02, //!< ESLR response message
  };

/**
 * The request type
 */
  enum EslrHeaderRequestType
  {
    NON = 0x00, //!< Non is requesting. This is set when ESLR header used to send KAM and SRC messages
    ET = 0x01, //!< Entire Table
    NE = 0x02, //!< Number of Entries specified in the ESLRRoutingHeader::NoE
    OE = 0x03, //!< Only one Entry 
    RESPOND = 0xff, //!< For all respond messages
  };

/**
 * Validity type of both neighbor and route records 
 */
enum Validity
{
  INVALID = 0x00, //!< Invalid Neighbor or Route Tecord
  VALID = 0x01, //!< Valid Neighbor or Route Record
  DISCONECTED = 0x02, //!< Disconnected Route Record
  LHOST = 0x03, //!< Host route for the loopback interface. This is not used for route advertisements
};

/**
  * Indicate whether the route record is presented in Main Table and Backup Table 
  */
enum RouteType
{
  PRIMARY = 0x01, //!< Record is added to the Primary Table
  SECONDARY = 0x02, //!< Record is in the Secondery Table
};

/**
  * Indicate whether the route record is presented in Main Table and Backup Table 
  */
enum Table
{
  MAIN = 0x01, //!< Record is added to the Primary Table
  BACKUP = 0x02, //!< Record is in the Secondery Table
};

enum UpdateType
{
  PERIODIC = 0x01, //!< ESLR Periodic Update
  TRIGGERED = 0x02, //!< ESLR Triggered Update
};

enum InvalidateType
{
  EXPIRE = 0x01, //!< Invaludate a route due to expiration of the route
  BROKEN = 0x02, //!< Invaludate a route due to broken link, broken route, or shutdown interface
};

/**
 * Split Horizon strategy type.
 */
enum SplitHorizonType {
  NO_SPLIT_HORIZON,//!< No Split Horizon
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
