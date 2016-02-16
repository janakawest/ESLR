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
#include "ns3/v4ping-helper.h"
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
  //std::string tFile ("src/eslr/examples/AS1221ELatencies.intra");
  std::string tFile ("src/eslr/examples/AS3967Elatencies.intra");

	double simTime = 1000;

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
    p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
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
  
  // -- Step 5
  //  -- create server nodes,
  //  -- set ISP router address,
  //  -- create udp echo server applications and attaches to the device
  //
  uint16_t portNumber = 9; // a well-known port number 
  Ptr<Ipv4> ipv4Server1 = nodes.Get (1)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server2 = nodes.Get (2)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server3 = nodes.Get (9)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server4 = nodes.Get (31)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server5 = nodes.Get (66)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server6 = nodes.Get (64)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server7 = nodes.Get (35)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server8 = nodes.Get (46)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server9 = nodes.Get (15)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server10 = nodes.Get (12)->GetObject<Ipv4> ();  
  Ptr<Ipv4> ipv4Server11 = nodes.Get (26)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Server12 = nodes.Get (78)->GetObject<Ipv4> ();   // 82 for 1221
  
  UdpEchoServerHelper server1 (portNumber);
  UdpEchoServerHelper server2 (portNumber);
  UdpEchoServerHelper server3 (portNumber);
  UdpEchoServerHelper server4 (portNumber);
  UdpEchoServerHelper server5 (portNumber);
  UdpEchoServerHelper server6 (portNumber);
  UdpEchoServerHelper server7 (portNumber);
  UdpEchoServerHelper server8 (portNumber);
  UdpEchoServerHelper server9 (portNumber);
  UdpEchoServerHelper server10 (portNumber);
  UdpEchoServerHelper server11 (portNumber);
  UdpEchoServerHelper server12 (portNumber);
  
  ApplicationContainer servers;
  
  //Server1 attributes
	server1.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server1->GetAddress (1, 0).GetLocal ()));
	server1.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server1->GetAddress (1, 0).GetMask ())); 
  servers = server1.Install (nodes.Get (1));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));  
  
  //Server2 attributes
	server2.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server2->GetAddress (1, 0).GetLocal ()));
	server2.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server2->GetAddress (1, 0).GetMask ())); 
  servers = server2.Install (nodes.Get (2));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));
  
  //Server3 attributes
	server3.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server3->GetAddress (1, 0).GetLocal ()));
	server3.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server3->GetAddress (1, 0).GetMask ())); 
  servers = server3.Install (nodes.Get (9));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));  
  
  //Server4 attributes
	server4.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server4->GetAddress (1, 0).GetLocal ()));
	server4.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server4->GetAddress (1, 0).GetMask ())); 
  servers = server4.Install (nodes.Get (31));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));
  
  //Server5 attributes
	server5.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server5->GetAddress (1, 0).GetLocal ()));
	server5.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server5->GetAddress (1, 0).GetMask ())); 
  servers = server5.Install (nodes.Get (66));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));  
  
  //Server6 attributes
	server6.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server6->GetAddress (1, 0).GetLocal ()));
	server6.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server6->GetAddress (1, 0).GetMask ())); 
  servers = server6.Install (nodes.Get (64));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));  

  //Server7 attributes
	server7.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server7->GetAddress (1, 0).GetLocal ()));
	server7.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server7->GetAddress (1, 0).GetMask ())); 
  servers = server7.Install (nodes.Get (35));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));   
  
  //Server8 attributes
	server8.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server8->GetAddress (1, 0).GetLocal ()));
	server8.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server8->GetAddress (1, 0).GetMask ())); 
  servers = server8.Install (nodes.Get (46));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));   
  
  //Server9 attributes
	server9.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server9->GetAddress (1, 0).GetLocal ()));
	server9.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server9->GetAddress (1, 0).GetMask ())); 
  servers = server9.Install (nodes.Get (15));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));     
  
  //Server10 attributes
	server10.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server10->GetAddress (1, 0).GetLocal ()));
	server10.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server10->GetAddress (1, 0).GetMask ())); 
  servers = server10.Install (nodes.Get (12));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));
  
  //Server11 attributes
	server11.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server11->GetAddress (1, 0).GetLocal ()));
	server11.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server11->GetAddress (1, 0).GetMask ())); 
  servers = server11.Install (nodes.Get (26));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));     
  
  //Server12 attributes
	server12.SetAttribute ("ServerAddress", Ipv4AddressValue (ipv4Server12->GetAddress (1, 0).GetLocal ()));
	server12.SetAttribute ("NetMask", Ipv4MaskValue (ipv4Server12->GetAddress (1, 0).GetMask ())); 
  servers = server12.Install (nodes.Get (70));
  servers.Start (Seconds (0.0));
  servers.Stop (Seconds (simTime));
  
  
  // -- Step 6
  //  -- select clients nodes (randomly),
  //  -- set server address,
  //  -- create udp echo client applications and attaches to the device
  //  
  
  uint32_t allNodes = nodes.GetN ();
   
	//create clientSet1
	UdpEchoClientHelper clientSet1 (ipv4Server1->GetAddress (1, 0).GetLocal (), portNumber);   
  clientSet1.SetAttribute ("MaxPackets", UintegerValue (1000000));
  clientSet1.SetAttribute ("Id", UintegerValue (1));
  //clientSet1.SetAttribute ("PacketSize", UintegerValue (1024));
  
	//create clientSet2
	UdpEchoClientHelper clientSet2 (ipv4Server5->GetAddress (1, 0).GetLocal (), portNumber);   
  clientSet2.SetAttribute ("MaxPackets", UintegerValue (1000000));
  clientSet2.SetAttribute ("Id", UintegerValue (1));
  //clientSet2.SetAttribute ("PacketSize", UintegerValue (1024));
  
	//create clientSet3
	UdpEchoClientHelper clientSet3 (ipv4Server8->GetAddress (1, 0).GetLocal (), portNumber);   
  clientSet3.SetAttribute ("MaxPackets", UintegerValue (1000000));

  clientSet3.SetAttribute ("Id", UintegerValue (1));
  //clientSet3.SetAttribute ("PacketSize", UintegerValue (1024));  
  
	//create clientSet4
	UdpEchoClientHelper clientSet4 (ipv4Server10->GetAddress (1, 0).GetLocal (), portNumber);   
  clientSet4.SetAttribute ("MaxPackets", UintegerValue (1000000));
  clientSet4.SetAttribute ("Id", UintegerValue (1));
  //clientSet4.SetAttribute ("PacketSize", UintegerValue (64));
  
	//create clientSet5
	UdpEchoClientHelper clientSet5 (ipv4Server3->GetAddress (1, 0).GetLocal (), portNumber);   
  clientSet5.SetAttribute ("MaxPackets", UintegerValue (1000000));
  clientSet5.SetAttribute ("Id", UintegerValue (1));
  //clientSet5.SetAttribute ("PacketSize", UintegerValue (1024));
  
	//create clientSet6
	UdpEchoClientHelper clientSet6 (ipv4Server9->GetAddress (1, 0).GetLocal (), portNumber);   
  clientSet6.SetAttribute ("MaxPackets", UintegerValue (10000000));
  clientSet6.SetAttribute ("Id", UintegerValue (1));
  //clientSet6.SetAttribute ("PacketSize", UintegerValue (1024));  
  
	//create clientSet7
/*	UdpEchoClientHelper clientSet7 (ipv4Server4->GetAddress (1, 0).GetLocal (), portNumber);   
  clientSet7.SetAttribute ("MaxPackets", UintegerValue (1000));
  clientSet7.SetAttribute ("Id", UintegerValue (1));
  clientSet7.SetAttribute ("PacketSize", UintegerValue (1024)); 
*/  

  uint16_t numberOfClients = 15;
  uint16_t frstClient = 0;
  NodeContainer firstSet;
  NodeContainer secondSet;
  NodeContainer thirdSet;  
  NodeContainer forthSet;  
  NodeContainer fifthSet;  
  NodeContainer sixthSet;     
  if (numberOfClients < (allNodes -1))
  {
    // Assign First set of clients
    for ( unsigned int i = 0; i < numberOfClients; i++, frstClient++ )
    {
      Ptr<Node> cNode = nodes.Get (frstClient);
      firstSet.Add (cNode);
    }
    
    frstClient = 15;
    numberOfClients = 20;
    // Assign Second set of clients
    for ( unsigned int i = 0; i < numberOfClients; i++, frstClient++ )
    {
      Ptr<Node> cNode2 = nodes.Get (frstClient);
      secondSet.Add (cNode2);
    }    
    
    frstClient = 30;
    numberOfClients = 10;    
    // Assign Third set of clients
    for ( unsigned int i = 0; i < numberOfClients; i++, frstClient++ )
    {
      Ptr<Node> cNode3 = nodes.Get (frstClient);
      thirdSet.Add (cNode3);
    }    
    
    frstClient = 58;
    numberOfClients = 12;    
    // Assign Fourth set of clients
    for ( unsigned int i = 0; i < numberOfClients; i++, frstClient++ )
    {
      Ptr<Node> cNode4 = nodes.Get (frstClient);
      forthSet.Add (cNode4);
    } 
    
    frstClient = 45;
    numberOfClients = 15;    
    // Assign Fourth set of clients
    for ( unsigned int i = 0; i < numberOfClients; i++, frstClient++ )
    {
      Ptr<Node> cNode5 = nodes.Get (frstClient);
      fifthSet.Add (cNode5);
    }          

    frstClient = 60;
    numberOfClients = 15;    
    // Assign Fourth set of clients
    for ( unsigned int i = 0; i < numberOfClients; i++, frstClient++ )
    {
      Ptr<Node> cNode6 = nodes.Get (frstClient);
      sixthSet.Add (cNode6);
    }      
  }
  else
  {
    std::cout << "you are allowed to set " << int (allNodes -1) << " number of clients" << std::endl;
    return -1;
  }  
  
  ApplicationContainer clients = clientSet1.Install (firstSet);    
  clientSet1.SetFill (clients.Get (0), "abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123");  
  clients.Start (Seconds (20.0));
  clients.Stop (Seconds (simTime));
  
  ApplicationContainer clients2 = clientSet2.Install (secondSet);    
  clientSet2.SetFill (clients2.Get (0), "abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123"); 
  clients2.Start (Seconds (30.0));
  clients2.Stop (Seconds (simTime)); 
  
  ApplicationContainer clients3 = clientSet3.Install (thirdSet);    
  clientSet3.SetFill (clients3.Get (0), "abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123");
  clients3.Start (Seconds (20.0));
  clients3.Stop (Seconds (simTime));  
  
  ApplicationContainer clients4 = clientSet4.Install (forthSet);    
  clientSet4.SetFill (clients4.Get (0), "abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123");
  clients4.Start (Seconds (100.0));
  clients4.Stop (Seconds (simTime)); 
  
  ApplicationContainer clients5 = clientSet5.Install (fifthSet);    
  clientSet5.SetFill (clients5.Get (0), "abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123");
  clients5.Start (Seconds (200.0));
  clients5.Stop (Seconds (simTime));       
  
  ApplicationContainer clients6 = clientSet6.Install (sixthSet);    
  clientSet6.SetFill (clients6.Get (0), "abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123abcdefghijklmnopqrstuvwxyz123");
  clients6.Start (Seconds (50.0));
  clients6.Stop (Seconds (simTime));
  
    
//  Ptr<V4Ping> app = CreateObject<V4Ping> ();
//  
//  
//  Ptr<Ipv4> ipv4Server = nodes.Get (31)->GetObject<Ipv4> ();
//  //Ptr<Ipv4> ipv4Server = nodes.Get (nodes.GetN () - 1)->GetObject<Ipv4> ();
//  app->SetAttribute ("Remote", Ipv4AddressValue (ipv4Server->GetAddress (1, 0).GetLocal ()));
//  app->SetAttribute ("Verbose", BooleanValue (true));

//  nodes.Get (26)->AddApplication (app);
//  //nodes.Get (2)->AddApplication (app);
//  
//  app->SetStartTime (Seconds (0.0));
//  app->SetStopTime (Seconds (simTime));

//  //
//  //  Select a Random server
//  //

//  uint32_t allNodes = nodes.GetN ();
//  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
//  rand->SetAttribute ("Min", DoubleValue (0));
//  rand->SetAttribute ("Max", DoubleValue (allNodes - 1));
//  
//  uint8_t randServerNum = rand->GetInteger (0, (allNodes - 1));

//  Ptr<Ipv4> ipv4Server1 = nodes.Get (randServerNum)->GetObject<Ipv4> ();
//  
//  V4PingHelper* ping4 =  new V4PingHelper (Ipv4Address (ipv4Server1->GetAddress (1, 0).GetLocal ()));
//  //ping4->SetAttribute ("Verbose", BooleanValue (true));
//  ping4->SetAttribute ("Size", UintegerValue (1250));
//  
//  NodeContainer clients;
//  // Specify number of clients we need
//  // keep in mind that we can add only clients less than ""number of nodes in the topology - 1""
//  
//  uint16_t numberOfClients = 50;
//  if (numberOfClients < (allNodes -1))
//  {
//    for ( unsigned int i = 0; i <= numberOfClients; i++ )
//    {
//      if (i != randServerNum)
//      {
//        Ptr<Node> cNode = nodes.Get (i);
//        clients.Add (cNode);
//      }
//    }
//  }
//  else
//  {
//    std::cout << "you are allowed to set " << int (allNodes -1) << " number of clients" << std::endl;
//    return -1;
//  }  
//  
//  ApplicationContainer apps = ping4->Install (clients);

//  apps.Start (Seconds (0.0));
//  apps.Stop (Seconds (simTime));
  
	Simulator::Stop (Seconds (simTime));    
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;            
    
}
