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
 * 
 * -- Topology information 
 * This program is written to read any RocketFuel topology file given as command-line argument.
 * Note that, however, this programs also developed based on the constraints and the limitations of topology reader module.
 * The servers, are implemented on 10 random nodes and several clients are also implemented in randomly
 * selected nodes. The clients maintain Possison Distribution process. We assume that all nodes, including servers, will
 * reserve data request in Possionly Distributed. 
 * ESLR is configured as the routing protocol. In addition, Clients and servers will use static routing to send 
 * data to their Gateway routers.
 */
 
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/eslr-module.h"


#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

#include "ns3/topology-read-module.h"
#include <list>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ESLRRocketFuel");

void MakeLinkDown (Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t intA, uint32_t intB)
{
	nodeA->GetObject<Ipv4>()->SetDown (intA);
	nodeB->GetObject<Ipv4>()->SetDown (intB);
}

void MakeLinkUp (Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t intA, uint32_t intB)
{
  nodeA->GetObject<Ipv4>()->SetUp (intA);
  nodeB->GetObject<Ipv4>()->SetUp (intB);
}

void MakeInterfaceDown (Ptr<Node> nodeA, uint32_t intA)
{
	nodeA->GetObject<Ipv4>()->SetDown (intA);
}

void MakeInterfaceUp (Ptr<Node> nodeA, uint32_t intA)
{
  nodeA->GetObject<Ipv4>()->SetUp (intA);
}

int main (int argc, char*argv[])
{
  bool verbose = true;
  bool MTable = false;
  bool BTable = false;
  bool NTable = false;
  
  // Topology format request
  std::string format ("Rocketfuel");
  std::string tFile ("src/eslr/examples/AS1221ELatencies.intra");

	double simTime = 3600;

  // Setup command line parameters used to control the simulation
  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("NTable", "Print the Neighbor Table", NTable);
  cmd.AddValue ("MTable", "Print the Main Routing Table", MTable);
  cmd.AddValue ("BTable", "Print the Backup Routing Table", BTable);
	cmd.AddValue ("SimTime", "Total Simulation Time", simTime);
	
	cmd.AddValue ("format","Format to use for data input [Orbis|Inet|Rocketfuel].", format);
  cmd.AddValue ("TopologyFile", "Name of the input file.", tFile);	

  cmd.Parse (argc,argv);

  //
  // --Step 1
  //  -- Read Topology information in the the given file.
  //  -- Read topology data.
  //  -- Pick a topology reader based on the requested format ([Orbis|Inet|Rocketfuel]).
  //
 	NS_LOG_INFO ("Read Topology Information.");

  // Pick a topology reader based in the requested format.
  TopologyReaderHelper topoHelp;
  topoHelp.SetFileName (tFile);
  topoHelp.SetFileType (format);
  Ptr<TopologyReader> topoFile = topoHelp.GetTopologyReader ();  

  NodeContainer nodes;
    
  if (topoFile != 0)
  {
    nodes = topoFile->Read ();
  }
  if (nodes.GetN () == 0)
  {
    NS_LOG_ERROR ("Problem reading the information of given topology file. Aborting...");
    std::cout << "Problem reading the information of given topology file. Aborting..." << std::endl;
    return -1;
  }
  if (topoFile->LinksSize () == 0)
  {
    NS_LOG_ERROR ("Problem reading given topology file. Aborting...");
    std::cout << "Problem reading given topology file. Aborting..." << std::endl;    
    return -1;    
  }
  NS_LOG_INFO ("RocketFuel network topology is creating using " << nodes.GetN () << " nodes and " << 
                topoFile->LinksSize () << " links according to the input file :" <<
                tFile);
  std::cout << "RocketFuel network topology is creating using " << int (nodes.GetN ()) << " nodes and " << 
                int(topoFile->LinksSize ()) << " links according to the input file: " <<
                tFile << std::endl;
  
  //
  // --Step 2
  //   --Create Nodes and and network stack
  //
  NS_LOG_INFO ("Creating Internet Stack and Assign Routing Protocols");
  InternetStackHelper internet;
  Ipv4ListRoutingHelper list;
  
  //Configure ESLR as only routing protocol
	EslrHelper eslrRouting;

  if (MTable)
    eslrRouting.Set ("PrintingMethod", EnumValue(eslr::MAIN_R_TABLE));
  else if (BTable)
  	eslrRouting.Set ("PrintingMethod", EnumValue(eslr::BACKUP_R_TABLE));
  else if (NTable) 
  	eslrRouting.Set ("PrintingMethod", EnumValue(eslr::N_TABLE));
  		
  list.Add (eslrRouting, 0);
 	internet.SetRoutingHelper (list);	  
 	
 	//Configure all nodes with the ESLR Protocol
	internet.Install (nodes); 	
  
  //
  // --Step 3 
  //  -- IP address assignment and configuration
  //
  NS_LOG_INFO ("Creating IPv4 Addresses");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.252");
  
  int totLinks = topoFile->LinksSize ();    
  
  NS_LOG_INFO ("Creating Node Containers");
  NodeContainer* nc = new NodeContainer[totLinks];
  TopologyReader::ConstLinksIterator iter;
  int i = 0;
  double linksWeight[totLinks];
  for (iter = topoFile->LinksBegin (); iter != topoFile->LinksEnd (); iter++, i++)
  {
    nc[i] = NodeContainer (iter->GetFromNode (), iter->GetToNode ());
    linksWeight[i] = iter->GetWeight ();
//  std::cout << iter->GetFromNode ()->GetId () << " " << iter->GetToNode ()->GetId () << " " <<  iter->GetWeight () << std::endl;
  }
  
  NS_LOG_INFO ("Creating Netdevice Containers");  
  NetDeviceContainer* ndc = new NetDeviceContainer[totLinks];
  PointToPointHelper p2p;
  for (int i=0; i< totLinks; i++)
  {
    // This part has to change according to the Discussion had with Erunika
    // Has to construct the entire shortest path from each device,
    // and determine the bandwidth requirement. 
    // The link delay has taken from the topology latancies
    p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    //p2p.SetChannelAttribute ("Delay", StringValue ("2ms")); // Transmission Delay is a guess
    p2p.SetChannelAttribute ("Delay", TimeValue(MilliSeconds(linksWeight[i])));
    ndc[i] = p2p.Install (nc[i]);    
  } 

  //
  // --Step 4 
  //  -- Create IP interfaces and create subnetworks between routers
  //
  //Creates one subnets between two nodes
  NS_LOG_INFO ("Creating IPv4 Interfaces");
  Ipv4InterfaceContainer* ipic = new Ipv4InterfaceContainer[totLinks];
	Ipv4Address tempAddress;

  for (int i = 0; i < totLinks; i++)
  {
    ipic[i] = ipv4.Assign (ndc[i]);

		//std::cout << ipv4.NewNetwork () << std::endl;
		ipv4.NewNetwork ();
  }
    
  
  NS_LOG_INFO ("Enable Printing Options.");
  EslrHelper routingHelper;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);


  if (MTable || NTable || BTable)
	{
        routingHelper.PrintRoutingTableEvery (Seconds (50), nodes.Get (9), routingStream);
	}   
  
//  Ptr<V4Ping> app = CreateObject<V4Ping> ();
//  
//  
//  Ptr<Ipv4> ipv4Server = nodes.Get (37)->GetObject<Ipv4> ();
//  //Ptr<Ipv4> ipv4Server = nodes.Get (nodes.GetN () - 1)->GetObject<Ipv4> ();
//  app->SetAttribute ("Remote", Ipv4AddressValue (ipv4Server->GetAddress (1, 0).GetLocal ()));
//  app->SetAttribute ("Verbose", BooleanValue (true));
//
//  nodes.Get (9)->AddApplication (app);
//  
//  app->SetStartTime (Seconds (0.0));
//  app->SetStopTime (Seconds (simTime));
 
  
	Simulator::Stop (Seconds (simTime));    
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;            
    
}
