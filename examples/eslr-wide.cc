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

// Network topology
//
// The network topology is implemented based on the backbone network of the WIDE Japan.
// The topology can be found in the following link. http://two.wide.ad.jp
// Note: the exact link details are simulated in this simulation. 
// However, for some constraints,
// we do not simulate the exact network addresses and client server placements as the WIDE network does.
// every router use ESLR as its routing protocol.
// Servers, and Clients are connected to respective routers using a default gateway.


#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/eslr-module.h"

#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/netanim-module.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WIDETestingNetwork");

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


int 
main (int argc, char *argv[])
{
  bool verbose = true;
  bool MTable = false;
  bool BTable = false;
  bool NTable = false;

	std::string nodeA = "NezuRouter";
 	std::string nodeB = "YagamiRouter";

	std::string nodeP = "KDDIOtemachiRouter";

	int nodeAInt = 4;
	int nodeBInt = 2;
	double simTime = 750;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
	cmd.AddValue ("PrintNode", "The node that Print its tables", nodeP);
  cmd.AddValue ("NTable", "Print the Neighbor Table", NTable);
  cmd.AddValue ("MTable", "Print the Main Routing Table", MTable);
  cmd.AddValue ("BTable", "Print the Backup Routing Table", BTable);
	cmd.AddValue ("SimTime", "Total Simulation Time", simTime);

	cmd.AddValue ("DisNodeA","Node 1 to disconnect", nodeA);
	cmd.AddValue ("AIntID","Node 1's Interface ID", nodeAInt);	
  cmd.AddValue ("DisNodeB","Node 2 to disconnect", nodeB);
	cmd.AddValue ("BIntID","Node 2's Interface ID", nodeBInt);

  cmd.Parse (argc,argv);

 	NS_LOG_INFO ("Create Routers.");
 	
  Ptr<Node> sendai = CreateObject<Node> (); // 1
  Names::Add ("SendaiRouter", sendai); 	
  Ptr<Node> tsukuba = CreateObject<Node> (); // 2 	
  Names::Add ("TsukubaRouter", tsukuba);
  Ptr<Node> nezu = CreateObject<Node> (); // 3
  Names::Add ("NezuRouter", nezu);    	 	
  Ptr<Node> kddiOtemachi = CreateObject<Node> (); // 4 	
  Names::Add ("KDDIOtemachiRouter", kddiOtemachi); 
  Ptr<Node> nttOtemachi = CreateObject<Node> (); // 5
  Names::Add ("NTTOtemachiRouter", nttOtemachi);     
 	Ptr<Node> shinkawasaki = CreateObject<Node> (); // 6 	
  Names::Add ("ShinKawasakiRouter", shinkawasaki);
 	Ptr<Node> yagami = CreateObject<Node> (); // 7
  Names::Add ("YagamiRouter", yagami);
 	Ptr<Node> fujisawa = CreateObject<Node> (); // 8
  Names::Add ("FujisawaRouter", fujisawa);      	
 	Ptr<Node> nara = CreateObject<Node> (); // 9
  Names::Add ("NaraRouter", nara); 
 	Ptr<Node> dojima = CreateObject<Node> (); // 10
  Names::Add ("DojimaRouter", dojima);  
  Ptr<Node> komatsu = CreateObject<Node> (); // 11
  Names::Add ("KomatsuRouter", komatsu);  
  Ptr<Node> sakyo = CreateObject<Node> (); // 12
  Names::Add ("SakyoRouter", sakyo);  
  Ptr<Node> hiroshima = CreateObject<Node> (); // 13 	
  Names::Add ("HiroshimaRouter", hiroshima);
  Ptr<Node> kurashiki = CreateObject<Node> (); // 14 	
  Names::Add ("KurashikiRouter", kurashiki);  
  Ptr<Node> fukuoka = CreateObject<Node> (); // 15 	
  Names::Add ("FukuokaRouter", fukuoka);   
  
 	NS_LOG_INFO ("Create Client Server Nodes.");
 	
	//clients
  Ptr<Node> c1 = CreateObject<Node> ();
  Names::Add ("Client1", c1);
  Ptr<Node> c2 = CreateObject<Node> ();
  Names::Add ("Client2", c2);
  Ptr<Node> c3 = CreateObject<Node> ();
  Names::Add ("Client3", c3);
  Ptr<Node> c4 = CreateObject<Node> ();
  Names::Add ("Client4", c4);
  Ptr<Node> c5 = CreateObject<Node> ();
  Names::Add ("Client5", c5);
  Ptr<Node> c6 = CreateObject<Node> ();
  Names::Add ("Client6", c6);
  Ptr<Node> c7 = CreateObject<Node> ();
  Names::Add ("Client7", c7);
  Ptr<Node> c8 = CreateObject<Node> ();
  Names::Add ("Client8", c8);
  Ptr<Node> c9 = CreateObject<Node> ();
  Names::Add ("Client9", c9);
  Ptr<Node> c10 = CreateObject<Node> ();
  Names::Add ("Client10", c10);
	
	//servers
	Ptr<Node> s1 = CreateObject<Node> ();
  Names::Add ("Server1", s1);
  Ptr<Node> s2 = CreateObject<Node> ();
  Names::Add ("Server2", s2);
  Ptr<Node> s3 = CreateObject<Node> ();
  Names::Add ("Server3", s3);	
  
 	NS_LOG_INFO ("Create links.");  
  // between routers
  NodeContainer net2 (sendai, nezu); // sendai --> i2, nezu --> i1
  NodeContainer net3 (nezu, kddiOtemachi); // nezu --> i2, kddiOtemachi --> i1
  NodeContainer net4 (nezu, yagami); // nezu --> i3, yagami --> i1
  NodeContainer net5 (nezu, dojima); // nezu --> i4, dojima --> i1
  NodeContainer net6 (kddiOtemachi, nttOtemachi); // kddiOtemachi --> i2, nttOtemachi --> i1
  NodeContainer net7 (yagami, shinkawasaki); // yagami --> i2, shinkawasaki --> i1
  NodeContainer net8 (yagami, fujisawa); // yagami --> i3, fujisawa --> i1
  NodeContainer net9 (nttOtemachi, fujisawa); // nttOtemachi --> i2, fujisawa --> i2  
  NodeContainer net10 (nttOtemachi, tsukuba); // nttOtemachi --> i3, tsukuba --> i1
  NodeContainer net11 (nttOtemachi, komatsu); // nttOtemachi --> i4, komatsu --> i1        
  NodeContainer net12 (nttOtemachi, dojima); // nttOtemachi --> i5, dojima --> i2    
  NodeContainer net13 (fujisawa, nara); // fujisawa --> i3, nara --> i1
  NodeContainer net14 (nara, sakyo); // nara --> i2, sakyo --> i1    
  NodeContainer net15 (nara, dojima); // nara --> i3, dojima --> i3
  NodeContainer net16 (dojima, sakyo); // dojima --> i4, sakyo --> i2
  NodeContainer net17 (dojima, hiroshima); // dojima --> i5, hiroshima --> i1
  NodeContainer net18 (dojima, kurashiki); // dojima --> i6, kurashiki --> i1 
  NodeContainer net19 (dojima, fukuoka); // dojima --> i7, fukuoka --> i1
  NodeContainer net20 (kurashiki, fukuoka); // kurashiki --> i2, fukuoka --> i2
  NodeContainer net21 (fukuoka, komatsu); // fukuoka --> i3, komatsu --> i2  
  
  //client<-->Router
  NodeContainer net1 (c1, sendai); // Sendai --> i1
  NodeContainer net23 (c2, sendai); // Sendai --> i3
  NodeContainer net24 (c3, nezu); // Nezu --> i5
  NodeContainer net25 (c4, tsukuba); // Tsukuba --> i2    
  NodeContainer net26 (c5, tsukuba); // Tsukuba --> i3
  NodeContainer net27 (c6, fujisawa); // fujisawa --> i4
  NodeContainer net28 (c7, shinkawasaki); // ShinKawasaki --> i2    
  NodeContainer net29 (c8, shinkawasaki); // ShinKawasaki --> i3
  NodeContainer net30 (c9, kddiOtemachi); // KDDIOtemachi --> i3
  NodeContainer net31 (c10, yagami); // Yagami --> i4      
    
  //server<-->Router
  NodeContainer net22 (s1, fukuoka); // fukuoka --> i4
  NodeContainer net32 (s2, hiroshima); // hiroshima --> i2  
  NodeContainer net33 (s3, sakyo); // sakyo --> i3
  
  NodeContainer routerSet1 (sendai, tsukuba, nezu, kddiOtemachi, nttOtemachi);
  NodeContainer routerSet2 (shinkawasaki, yagami, fujisawa, nara, dojima);
  NodeContainer routerSet3 (komatsu, sakyo, hiroshima, kurashiki, fukuoka); 
  NodeContainer routers (routerSet1, routerSet2, routerSet3);
  NodeContainer clientSet1 (c1,c2,c3,c4,c5);
  NodeContainer clientSet2 (c6,c7,c8,c9,c10);
  NodeContainer serverSet1 (s1,s2,s3);
  NodeContainer nodes (clientSet1, clientSet2, serverSet1);
  
	NS_LOG_INFO ("Create channels.");
	
	NS_LOG_INFO ("Set 10Gbps Links.");	
	PointToPointHelper p2p_10Gbps;
  p2p_10Gbps.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  p2p_10Gbps.SetChannelAttribute ("Delay", StringValue ("2ms")); // Transmission Delay is a guess

  NetDeviceContainer ndc3 = p2p_10Gbps.Install (net3); //net3 nezu --> i2, kddiOtemachi --> i1
  NetDeviceContainer ndc4 = p2p_10Gbps.Install (net4); //net4 nezu --> i3, yagami --> i1
  NetDeviceContainer ndc5 = p2p_10Gbps.Install (net5); //net5 nezu --> i4, dojima --> i1
  NetDeviceContainer ndc6 = p2p_10Gbps.Install (net6); //net6 kddiOtemachi --> i2, nttOtemachi --> i1
  NetDeviceContainer ndc7 = p2p_10Gbps.Install (net7); //net7 yagami --> i2, shinkawasaki --> i1
  NetDeviceContainer ndc8 = p2p_10Gbps.Install (net8); //net8 yagami --> i3, fujisawa --> i1
  NetDeviceContainer ndc9 = p2p_10Gbps.Install (net9); //net9 nttOtemachi --> i2, fujisawa --> i2  

  NetDeviceContainer ndc11 = p2p_10Gbps.Install (net11); //net11 nttOtemachi --> i4, komatsu --> i1        
  NetDeviceContainer ndc12 = p2p_10Gbps.Install (net12); //net12 nttOtemachi --> i5, dojima --> i2    
  
  NetDeviceContainer ndc15 = p2p_10Gbps.Install (net15); //net15 nara --> i3, dojima --> i3
  

  NetDeviceContainer ndc18 = p2p_10Gbps.Install (net18); //net18 dojima --> i6, kurashiki --> i1 
  NetDeviceContainer ndc19 = p2p_10Gbps.Install (net19); //net19 dojima --> i7, fukuoka --> i1
  NetDeviceContainer ndc20 = p2p_10Gbps.Install (net20); //net20 kurashiki --> i2, fukuoka --> i2
  NetDeviceContainer ndc21 = p2p_10Gbps.Install (net21); //net21 fukuoka --> i3, komatsu --> i2  

	NS_LOG_INFO ("Set 1Gbps Links.");
  PointToPointHelper p2p_1Gbps;
  p2p_1Gbps.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2p_1Gbps.SetChannelAttribute ("Delay", StringValue ("2ms")); // Transmission Delay is a guess 

  NetDeviceContainer ndc1 = p2p_1Gbps.Install (net1);// net1 Sendai --> i1
  NetDeviceContainer ndc2 = p2p_1Gbps.Install (net2);// net2 sendai --> i2, nezu --> i1    
  NetDeviceContainer ndc14 = p2p_1Gbps.Install (net14); //net14 nara --> i2, sakyo --> i1    
  NetDeviceContainer ndc16 = p2p_1Gbps.Install (net16); // net16 dojima --> i4, sakyo --> i2  
  NetDeviceContainer ndc22 = p2p_1Gbps.Install (net22); //net22 fukuoka --> i4
  NetDeviceContainer ndc32 = p2p_1Gbps.Install (net32); //net32 hiroshima --> i2
  NetDeviceContainer ndc33 = p2p_1Gbps.Install (net33); //net33 sakyo --> i3

	NS_LOG_INFO ("Set 100Mbps Links.");  
  PointToPointHelper p2p_100Mbps;
  p2p_100Mbps.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p_100Mbps.SetChannelAttribute ("Delay", StringValue ("2ms"));	// Transmission Delay is a guess   

  NetDeviceContainer ndc10 = p2p_100Mbps.Install (net10);// net10 nttOtemachi --> i3, tsukuba --> i1  
  NetDeviceContainer ndc13 = p2p_100Mbps.Install (net13);// net13 fujisawa --> i3, nara --> i1
  NetDeviceContainer ndc17 = p2p_100Mbps.Install (net17); //net17 dojima --> i5, hiroshima --> i1 
  
  NetDeviceContainer ndc23 = p2p_100Mbps.Install (net23); //net23 sendai --> i3
  NetDeviceContainer ndc24 = p2p_100Mbps.Install (net24); //net24 Nezu --> i5
  NetDeviceContainer ndc25 = p2p_100Mbps.Install (net25); //net25 Tsukuba --> i2 
  NetDeviceContainer ndc26 = p2p_100Mbps.Install (net26); //net26 Tsukuba --> i3 
  NetDeviceContainer ndc27 = p2p_100Mbps.Install (net27); //net27 fujisawa --> i4
  NetDeviceContainer ndc28 = p2p_100Mbps.Install (net28); //net28 ShinKawasaki --> i2
  NetDeviceContainer ndc29 = p2p_100Mbps.Install (net29); //net29 ShinKawasaki --> i3
  NetDeviceContainer ndc30 = p2p_100Mbps.Install (net30); //net30 KDDIOtemachi --> i3
  NetDeviceContainer ndc31 = p2p_100Mbps.Install (net31); //net31 Yagami --> i4


	NS_LOG_INFO ("Create IPv4 and routing");
	EslrHelper eslrRouting;

  // Rule of thumb:
  // Interfaces are added sequentially, starting from 0
  // However, interface 0 is always the loopback...
  eslrRouting.ExcludeInterface (sendai, 1);
  eslrRouting.ExcludeInterface (fukuoka, 4);

  if (MTable)
    eslrRouting.Set ("PrintingMethod", EnumValue(eslr::MAIN_R_TABLE));
  else if (BTable)
  	eslrRouting.Set ("PrintingMethod", EnumValue(eslr::BACKUP_R_TABLE));
  else if (NTable) 
  	eslrRouting.Set ("PrintingMethod", EnumValue(eslr::N_TABLE));

  Ipv4ListRoutingHelper list;
  list.Add (eslrRouting, 0);

	InternetStackHelper internet;
 	internet.SetRoutingHelper (list);
	internet.Install (routers);

	InternetStackHelper internetNodes;
	internetNodes.Install (nodes);
	
	NS_LOG_INFO ("Assign IPv4 Addresses.");
	Ipv4AddressHelper ipv4;
	
	//For Client Networks
	ipv4.SetBase ("192.168.16.0","255.255.255.252");
	Ipv4InterfaceContainer iic1 = ipv4.Assign (ndc1);	// Sendai,i1 <--> client1,1
	ipv4.SetBase ("172.16.15.0","255.255.255.252");
	Ipv4InterfaceContainer iic23 = ipv4.Assign (ndc23);	// Sendai,i3 <--> client2,1	
	ipv4.SetBase ("138.16.10.0","255.255.255.252");
	Ipv4InterfaceContainer iic24 = ipv4.Assign (ndc24);	// nezu,i5 <--> client3,1
	ipv4.SetBase ("11.10.10.0","255.255.255.252");
	Ipv4InterfaceContainer iic25 = ipv4.Assign (ndc25);	// tsukuba,i2 <--> client4,1	
	ipv4.SetBase ("12.11.10.0","255.255.255.252");
	Ipv4InterfaceContainer iic26 = ipv4.Assign (ndc26);	// tsukuba,i3 <--> client5,1	
	ipv4.SetBase ("10.0.10.0","255.255.255.252");
	Ipv4InterfaceContainer iic27 = ipv4.Assign (ndc27);	// fujisawa,i4 <--> client6,1
	ipv4.SetBase ("192.165.10.0","255.255.255.252");
	Ipv4InterfaceContainer iic28 = ipv4.Assign (ndc28);	// shinkawasaki,i3 <--> client7,1	
	ipv4.SetBase ("130.10.10.0","255.255.255.252");
	Ipv4InterfaceContainer iic29 = ipv4.Assign (ndc29);	// shinkawasaki,i4 <--> client8,1
	ipv4.SetBase ("172.10.10.0","255.255.255.252");
	Ipv4InterfaceContainer iic30 = ipv4.Assign (ndc30);	// kddiOtemachi,i3 <--> client9,1	
	ipv4.SetBase ("10.1.1.0","255.255.255.252");
	Ipv4InterfaceContainer iic31 = ipv4.Assign (ndc31);	// yagami,i4 <--> client10,1								
	
	// for the internal Network
	ipv4.SetBase ("203.178.136.228","255.255.255.252");
		Ipv4InterfaceContainer iic2 = ipv4.Assign (ndc2);	// sendai,i2 <--> nezu,i1
	ipv4.SetBase ("203.178.136.220","255.255.255.252");
		Ipv4InterfaceContainer iic3 = ipv4.Assign (ndc3);	//nezu,i2 <--> kddiOtemachi,i1	
  ipv4.SetBase ("203.178.136.92","255.255.255.252");
  	Ipv4InterfaceContainer iic4 = ipv4.Assign (ndc4);	//nezu,i3 <--> yagami,i1
  ipv4.SetBase ("203.178.136.72","255.255.255.252");
  	Ipv4InterfaceContainer iic5 = ipv4.Assign (ndc5);	//nezu,i4 <--> dojima,i1
	ipv4.SetBase ("203.178.138.0","255.255.255.0");
		Ipv4InterfaceContainer iic6 = ipv4.Assign (ndc6);	//kddiOtemachi,i2 <--> nttOtemachi,i1
  ipv4.SetBase ("203.178.136.244","255.255.255.252");
  	Ipv4InterfaceContainer iic7 = ipv4.Assign (ndc7);	//yagami,i2 <--> shinkawasaki,i1
	ipv4.SetBase ("203.178.137.64","255.255.255.224");	
		Ipv4InterfaceContainer iic8 = ipv4.Assign (ndc8);	//yagami,i3 <--> fujisawa,i1
	ipv4.SetBase ("202.244.32.248","255.255.255.252");
		Ipv4InterfaceContainer iic9 = ipv4.Assign (ndc9);	//nttOtemachi,i2 <--> fujisawa,i2  	
	ipv4.SetBase ("203.178.136.204","255.255.255.252");	
		Ipv4InterfaceContainer iic10 = ipv4.Assign (ndc10); //nttOtemachi,i3 <--> tsukuba,i1  
	ipv4.SetBase ("203.178.138.208","255.255.255.248");					
		Ipv4InterfaceContainer iic11 = ipv4.Assign (ndc11); //nttOtemachi,i4 <--> komatsu,i1 
	ipv4.SetBase ("203.178.141.224","255.255.255.224");
		Ipv4InterfaceContainer iic12 = ipv4.Assign (ndc12);	//nttOtemachi,i5 <--> dojima,i2    	
	ipv4.SetBase ("203.178.136.184","255.255.255.252");	
		Ipv4InterfaceContainer iic13 = ipv4.Assign (ndc13);	//fujisawa,i3 <--> nara,i1  
	ipv4.SetBase ("203.178.138.164","255.255.255.252");
		Ipv4InterfaceContainer iic14 = ipv4.Assign (ndc14);	//nara,i2 <--> sakyo,i1    
	ipv4.SetBase ("202.244.138.224","255.255.255.224");
		Ipv4InterfaceContainer iic15 = ipv4.Assign (ndc15);	//nara,i3 <--> dojima,i3	
	ipv4.SetBase ("203.178.138.96","255.255.255.224");	
		Ipv4InterfaceContainer iic16 = ipv4.Assign (ndc16);	//dojima,i4 <--> sakyo,i2  
	ipv4.SetBase ("203.178.140.192","255.255.255.224");					
		Ipv4InterfaceContainer iic17 = ipv4.Assign (ndc17); //dojima,i5 <--> hiroshima,i1
	ipv4.SetBase ("203.178.136.196","255.255.255.252");
		Ipv4InterfaceContainer iic18 = ipv4.Assign (ndc18);	//dojima,i6 <--> kurashiki,i1	
	ipv4.SetBase ("203.178.136.232","255.255.255.252");	
		Ipv4InterfaceContainer iic19 = ipv4.Assign (ndc19);	//dojima,i7 <--> fukuoka,i1
	ipv4.SetBase ("203.178.138.200","255.255.255.252");			
		Ipv4InterfaceContainer iic20 = ipv4.Assign (ndc20); //kurashiki,i2 <--> fukuoka,i2
	ipv4.SetBase ("203.178.140.224","255.255.255.224");			
		Ipv4InterfaceContainer iic21 = ipv4.Assign (ndc21);	//fukuoka,i3 <--> komatsu,i2  	
			
	//For Servers
  ipv4.SetBase ("15.16.16.0","255.255.255.252");
  Ipv4InterfaceContainer iic22 = ipv4.Assign (ndc22); //fukuoka,i4 <-->	server1,i1
  ipv4.SetBase ("124.12.10.0","255.255.255.252");
  Ipv4InterfaceContainer iic32 = ipv4.Assign (ndc32); //hiroshima,i2 <-->	server2,i1  
  ipv4.SetBase ("173.252.120.0","255.255.255.252");
  Ipv4InterfaceContainer iic33 = ipv4.Assign (ndc33); //sakyo,i3 <-->	server3,i1      
	
	 
  NS_LOG_INFO ("Setting the default gateways of the Source and Destination.");
  Ipv4StaticRoutingHelper statRouting;
  
	// setting up the 'Sendai' as the default gateway of the 'C1' 
	Ptr<Ipv4StaticRouting> statC1 = statRouting.GetStaticRouting (c1->GetObject<Ipv4> ());
	statC1->SetDefaultRoute (sendai->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), 1, 1);
	// setting up the 'Sendai' as the default gateway of the 'C2' 
	Ptr<Ipv4StaticRouting> statC2 = statRouting.GetStaticRouting (c2->GetObject<Ipv4> ());
	statC2->SetDefaultRoute (sendai->GetObject<Ipv4> ()->GetAddress (3, 0).GetLocal (), 1, 1);
	// setting up the 'Nezu' as the default gateway of the 'C3' 
	Ptr<Ipv4StaticRouting> statC3 = statRouting.GetStaticRouting (c3->GetObject<Ipv4> ());
	statC3->SetDefaultRoute (nezu->GetObject<Ipv4> ()->GetAddress (5, 0).GetLocal (), 1, 1);
	// setting up the 'Tsukuba' as the default gateway of the 'C4' 
	Ptr<Ipv4StaticRouting> statC4 = statRouting.GetStaticRouting (c4->GetObject<Ipv4> ());
	statC4->SetDefaultRoute (tsukuba->GetObject<Ipv4> ()->GetAddress (2, 0).GetLocal (), 1, 1);	
	// setting up the 'Tsukuba' as the default gateway of the 'C5' 
	Ptr<Ipv4StaticRouting> statC5 = statRouting.GetStaticRouting (c5->GetObject<Ipv4> ());
	statC5->SetDefaultRoute (tsukuba->GetObject<Ipv4> ()->GetAddress (3, 0).GetLocal (), 1, 1);	
	// setting up the 'Fujisawa' as the default gateway of the 'C6' 
	Ptr<Ipv4StaticRouting> statC6 = statRouting.GetStaticRouting (c6->GetObject<Ipv4> ());
	statC6->SetDefaultRoute (fujisawa->GetObject<Ipv4> ()->GetAddress (4, 0).GetLocal (), 1, 1);	
	// setting up the 'ShinKawasaki' as the default gateway of the 'C7' 
	Ptr<Ipv4StaticRouting> statC7 = statRouting.GetStaticRouting (c7->GetObject<Ipv4> ());
	statC7->SetDefaultRoute (shinkawasaki->GetObject<Ipv4> ()->GetAddress (2, 0).GetLocal (), 1, 1);	
	// setting up the 'ShinKawasaki' as the default gateway of the 'C8' 
	Ptr<Ipv4StaticRouting> statC8 = statRouting.GetStaticRouting (c8->GetObject<Ipv4> ());
	statC8->SetDefaultRoute (shinkawasaki->GetObject<Ipv4> ()->GetAddress (3, 0).GetLocal (), 1, 1);	
	// setting up the 'KDDIOtemachi' as the default gateway of the 'C8' 
	Ptr<Ipv4StaticRouting> statC9= statRouting.GetStaticRouting (c9->GetObject<Ipv4> ());
	statC9->SetDefaultRoute (kddiOtemachi->GetObject<Ipv4> ()->GetAddress (3, 0).GetLocal (), 1, 1);	
	// setting up the 'Yagami' as the default gateway of the 'C10' 
	Ptr<Ipv4StaticRouting> statC10 = statRouting.GetStaticRouting (c10->GetObject<Ipv4> ());
	statC10->SetDefaultRoute (yagami->GetObject<Ipv4> ()->GetAddress (4, 0).GetLocal (), 1, 1);	
	
	
  // setting up the 'Fukuoka' as the default gateway of the 'S1'
	Ptr<Ipv4StaticRouting> statS1 = statRouting.GetStaticRouting (s1->GetObject<Ipv4> ());
	statS1->SetDefaultRoute (fukuoka->GetObject<Ipv4> ()->GetAddress (4, 0).GetLocal (), 1, 1);
  // setting up the 'Hiroshima' as the default gateway of the 'S2'
	Ptr<Ipv4StaticRouting> statS2 = statRouting.GetStaticRouting (s2->GetObject<Ipv4> ());
	statS2->SetDefaultRoute (hiroshima->GetObject<Ipv4> ()->GetAddress (2, 0).GetLocal (), 1, 1);	
  // setting up the 'sakyo' as the default gateway of the 'S3'
	Ptr<Ipv4StaticRouting> statS3 = statRouting.GetStaticRouting (s3->GetObject<Ipv4> ());
	statS3->SetDefaultRoute (sakyo->GetObject<Ipv4> ()->GetAddress (3, 0).GetLocal (), 1, 1);	
  
  NS_LOG_INFO ("Setting up UDP echo server and client.");
	//create server1	
	uint16_t port = 9; // well-known echo port number
	UdpEchoServerHelper server1 (port);
	server1.SetAttribute ("ServerAddress", Ipv4AddressValue (s1->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ()));
	server1.SetAttribute ("NetMask", Ipv4MaskValue (s1->GetObject<Ipv4> ()->GetAddress (1, 0).GetMask ()));	
	server1.SetAttribute ("ISPAddress", Ipv4AddressValue (fukuoka->GetObject<Ipv4> ()->GetAddress (4, 0).GetLocal ()));		
	ApplicationContainer apps = server1.Install (s1); 	
	apps.Start (Seconds (10.0));
	apps.Stop (Seconds (3599.0));

	//create server2	
	UdpEchoServerHelper server2 (port);
	server2.SetAttribute ("ServerAddress", Ipv4AddressValue (s2->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ()));
	server2.SetAttribute ("NetMask", Ipv4MaskValue (s2->GetObject<Ipv4> ()->GetAddress (1, 0).GetMask ()));	
	server2.SetAttribute ("ISPAddress", Ipv4AddressValue (hiroshima->GetObject<Ipv4> ()->GetAddress (2, 0).GetLocal ()));		
	apps = server2.Install (s2); 	
	apps.Start (Seconds (10.0));
	apps.Stop (Seconds (3599.0));	
	
	//create server3	
	UdpEchoServerHelper server3 (port);
	server3.SetAttribute ("ServerAddress", Ipv4AddressValue (s3->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ()));
	server3.SetAttribute ("NetMask", Ipv4MaskValue (s3->GetObject<Ipv4> ()->GetAddress (1, 0).GetMask ()));	
	server3.SetAttribute ("ISPAddress", Ipv4AddressValue (sakyo->GetObject<Ipv4> ()->GetAddress (3, 0).GetLocal ()));		
	apps = server3.Install (s3); 	
	apps.Start (Seconds (10.0));
	apps.Stop (Seconds (3599.0));	
	
/*	
	//create client1
	UdpEchoClientHelper client1 (s1->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client1.SetAttribute ("MaxPackets", UintegerValue (1000));
  client1.SetAttribute ("Id", UintegerValue (1));
  client1.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client1.Install (c1);  
  apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));
	
	//create client2
	UdpEchoClientHelper client2 (s2->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client2.SetAttribute ("MaxPackets", UintegerValue (1000));
  client2.SetAttribute ("Id", UintegerValue (2));
  client2.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client2.Install (c2);  
  apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));	
	
	//create client3
	UdpEchoClientHelper client3 (s3->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client3.SetAttribute ("MaxPackets", UintegerValue (1000));
  client3.SetAttribute ("Id", UintegerValue (3));
  client3.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client3.Install (c3);  
  apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));
	
	//create client4
	UdpEchoClientHelper client4 (s3->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client4.SetAttribute ("MaxPackets", UintegerValue (1000));
  client4.SetAttribute ("Id", UintegerValue (4));
  client4.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client4.Install (c4);  
  apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));	
	
	//create client5
	UdpEchoClientHelper client5 (s1->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client5.SetAttribute ("MaxPackets", UintegerValue (1000));
  client5.SetAttribute ("Id", UintegerValue (5));
  client5.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client5.Install (c5);  
  apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));
		
	//create client6
	UdpEchoClientHelper client6 (s1->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client6.SetAttribute ("MaxPackets", UintegerValue (1000));
  client6.SetAttribute ("Id", UintegerValue (6));
  client6.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client6.Install (c6);  
  apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));		
	
	//create client7
	UdpEchoClientHelper client7 (s2->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client7.SetAttribute ("MaxPackets", UintegerValue (1000));
  client7.SetAttribute ("Id", UintegerValue (7));
  client7.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client7.Install (c7);  
  apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));	
	
	//create client8
	UdpEchoClientHelper client8 (s2->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client8.SetAttribute ("MaxPackets", UintegerValue (1000));
  client8.SetAttribute ("Id", UintegerValue (8));
  client8.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client8.Install (c8);  
  apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));	
	
	//create client9
	UdpEchoClientHelper client9 (s2->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client9.SetAttribute ("MaxPackets", UintegerValue (1000));
  client9.SetAttribute ("Id", UintegerValue (9));
  client9.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client9.Install (c9);  
  apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));	
	
	//create client10
	UdpEchoClientHelper client10 (s1->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port); 
  client10.SetAttribute ("MaxPackets", UintegerValue (1000));
  client10.SetAttribute ("Id", UintegerValue (10));
  client10.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client10.Install (c10); 
	apps.Start (Seconds (20.0));
	apps.Stop (Seconds (350.0));												
*/	
  //NS_LOG_INFO ("Enable Printing Options.");
  EslrHelper routingHelper;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);


	Ptr<Node> pNode = Names::Find<Node> (nodeP);
  if (MTable || NTable || BTable)
	{
//		for (uint8_t num = 0; num < routers.GetN (); num ++)
//		{
//			routingHelper.PrintRoutingTableAt (Seconds (304), routers.Get (num), routingStream);
//			routingHelper.PrintRoutingTableAt (Seconds (350), routers.Get (num), routingStream);			
//			routingHelper.PrintRoutingTableAt (Seconds (450), routers.Get (num), routingStream);			
//			routingHelper.PrintRoutingTableAt (Seconds (552), routers.Get (num), routingStream);			
//		}
        routingHelper.PrintRoutingTableEvery (Seconds (50), pNode, routingStream);
	} 	

			Ptr<Node> dNodeA = Names::Find<Node> (nodeA);
			Ptr<Node> dNodeB = Names::Find<Node> (nodeB);
      Simulator::Schedule (Seconds (300), &MakeLinkDown, dNodeA, dNodeB, 4, 2); 
      Simulator::Schedule (Seconds (550), &MakeLinkUp, dNodeA, dNodeB, 4, 2);
//      
//      Simulator::Schedule (Seconds (410), &MakeLinkDown, b, c, 2, 2); 
//      Simulator::Schedule (Seconds (550), &MakeLinkUp, b, c, 2, 2); 
//      
//      Simulator::Schedule (Seconds (420), &MakeLinkDown, b, a, 1, 2); 
//      Simulator::Schedule (Seconds (550), &MakeLinkUp, b, a, 1, 2);  

//      Simulator::Schedule (Seconds (40), &MakeInterfaceDown, b, 3); 
//      Simulator::Schedule (Seconds (185), &MakeInterfaceUp, b, 3); 
      

	Simulator::Stop (Seconds (simTime));
	AnimationInterface anim ("WIDE_Animation.xml");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}


