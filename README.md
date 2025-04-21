COSC402 Assignment 2 README

The assignment is to implement a sensor network using Contiki and Cooja. The network consists of a sink node and multiple sensor nodes. The sensor nodes measure lightness every minute and temperature data every five minutes, and send the data to the sink node using shortest path multi-hop routing based on hop-count.

There are four files for COSC402 Assignment 2:
1. sink_node.c.         Source code for the sink node
2. sensor_node.c        Source code for sensor nodes
3. Assignment2.csc      Cooja simulation file
4. Makefile             Makefile for compiling the programme

To run the programme, place all these files in a folder under contiki directory in Linux. Open a terminal, direct to the directory contiki/tools/cooja, and run the command "ant run" to open Cooja. In Cooja, click "File - Open simulation - Browse...". Then select Assignment2.csc and the simulation can be loaded. There are 1 sink node(nodeID 1) and 8 sensor nodes(nodeIDs 2 to 9) in the network panel. Click the start button in the simulation control panel and the simulation starts running.

After 1 minute, all sensor nodes will send both lightness and temperature data to the sink node with shortest path routing based on hop-count.

In the mote output window, the log of one transmission process will be printed as follows:

01:05.023	ID:9	Send packet "9,140,24" from 9 to 1...
01:05.027	ID:9	My nodeId 9: Forwarding packet to nodeId 2 (2 in list), hops 1
01:05.142	ID:2	My nodeId 2: Forwarding packet to nodeId 4 (4 in list), hops 2
01:05.254	ID:4	My nodeId 4: Forwarding packet to nodeId 1 (2 in list), hops 3
01:05.285	ID:1	nodeID     light      temperature
01:05.288	ID:1	9          140        24        

The logs show how packets are sent from sensor nodes to the sink node, and then the sink node will print the results in a specific format. For this example, sensor node 9 sends a data packet to sink node 1 through the intermediate sensor nodes 2 and 4. The sink node prints the nodeID, light, and temperature sent from sensor node 9.

At the 2nd, 3rd, 4th, and 5th minute, the sensor nodes only send lightness data but not temperature data, and the sink node prints the following results:

02:07.999	ID:9	Send packet "9,160" from 9 to 1...
02:08.003	ID:9	My nodeId 9: Forwarding packet to nodeId 2 (2 in list), hops 1
02:08.141	ID:2	My nodeId 2: Forwarding packet to nodeId 4 (4 in list), hops 2
02:08.255	ID:4	My nodeId 4: Forwarding packet to nodeId 1 (2 in list), hops 3
02:08.283	ID:1	nodeID     light     
02:08.285	ID:1	9          160  

At the 6th(and 11, 16, 21...) minute, the sensor nodes get the temperature data again, and send both lightness and temperature data to the sink node.

The programmes should run without warnings and errors.


We can also create a new simulation by clicking "File - New simulation". Then create a sink node by clicking "Motes - Add motes - Create new mote type - Sky mote...". Click "Browse" and select sink_node.c. Click "Compile" and "Create" to create a sink node with ID 1. 

After creating the sink node, we can follow the same steps to create multiple sensor nodes. Click "Motes - Add motes - Create new mote type - Sky mote...", then click "Browse" and select sensor_node.c. Click "Compile" and "Create" to create multiple sensor nodes.

Move the sink node and sensor nodes so that they can contact each other. Click "Start" in the simulation control panel. The sensor nodes will send lightness data every minute and temperature data every 5 minutes to the sink node, using multi-hop routing with the minimum number of hop-count, and the sink node will print the data in a specific format.




