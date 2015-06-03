
ESLR implementation and the readme file is yet to create
========================================================

# ESLR
Server Link Router state routing protocol.

This is developed as a ns-3 module

In some case, if some one found this repository and try to use it, please be noted that this module is under constrction. 
To be highlighted, this protocol is still under development. 

However, at the moment, the routing protocol is capable of:
->	Route table genaration (both Main and Backup tables)
->	Discover neighbor nodes and maintain a neighbor table
->	Request routing tables from neighbor
->	Route advertisement (Periodic and Triggered)
->	Route management including Split-Horizon and Route Poisoning
->	Maintain the minimum delay path as the best route.
->	Maintain next best delay path as the backup route.

Note that, in order to calculate the route cost (minimum delay path), you are require to update your ns3 node to buffer all incoming packets. To that, you have to follow https://github.com/janakawest/ns3nodepacketbuffer. Without implementing the packet buffer, every route will have 0 as the cost of the route.

There are isevaral bugs yet to be fixed. The most common bug is "a segmentation fault while printing the Main table". 
The current implementation is done on the ns-3.21 for a simple topology as explained in the RIPv2 RFC.

Please backup your ns-3 system before add and execute the module. Please be noted that "use this module at your own risk."

In addition, if some one is looking for a Distance vector Routing protocl, please visit: https://github.com/westlab/ns3dvrp

