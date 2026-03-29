# Sensor Network – Contiki OS & Cooja in C Language

This assignment implements a **sensor network** using **Contiki OS** and **Cooja**. The network consists of a **sink node** and multiple **sensor nodes**. Sensor nodes measure **lightness every minute** and **temperature every 5 minutes**, sending data to the sink node via **shortest-path multi-hop routing** based on hop count.

---

## Files

- `sink_node.c` – Sink node source code
- `sensor_node.c` – Sensor node source code
- `Assignment2.csc` – Cooja simulation file
- `Makefile` – Compile the program

---

## Running the Simulation

1. Place all files in a folder under the Contiki directory on Linux.
2. Open a terminal, navigate to `contiki/tools/cooja`, and run:
```bash
   ant run
```
3. In Cooja, open the simulation:
```
   File → Open simulation → Browse → Assignment2.csc
```
4. The network panel will show 1 sink node (ID:1) and 8 sensor nodes (IDs:2–9).
5. Click **Start** in the simulation control panel.

---

## How the Code Works

### Sensor Node (`sensor_node.c`)

- Maintains a neighbor table with hop counts to the sink.
- Periodically announces its hop count to neighbors (`ANNOUNCEMENT_INTERVAL`) using the Contiki announcement module.
- Updates its own hop count as `min(neighbor hop counts) + 1`.
- Measures light every minute and temperature every 5 minutes.
- Sends data to sink node (ID:1) via multi-hop routing, selecting the neighbor with the lowest hop count.
- Randomly delays sending (0–10 seconds) to reduce packet collisions.
- Prints forwarding logs showing the path each packet takes in Cooja.

### Sink Node (`sink_node.c`)

- Maintains a multihop connection, but does not forward packets or maintain a neighbor table.
- Receives packets and prints data in a structured format:
```
  nodeID    light    temperature
  9         140      24
```
- Periodically announces a dummy value to help sensor nodes compute hop counts.

---

## Overall Workflow

1. Sensor nodes discover neighbors and calculate shortest-path hop count to the sink.
2. Nodes send light data every minute and temperature data every 5 minutes.
3. Packets are forwarded hop-by-hop to the sink along the minimum hop-count path.
4. The sink prints received data in a clear table format.
5. Cooja logs show both packet transmissions and forwarding paths, providing visibility into routing.

---

## Screenshot

<img width="1086" height="874" alt="Random_no_packet_loss" src="https://github.com/user-attachments/assets/2759b5a8-338a-4db9-8b03-4458fa2f5f86" />

<img width="1086" height="829" alt="Random_no_packet_loss2" src="https://github.com/user-attachments/assets/7aad3eea-6495-4d14-a873-5b2ef44219b1" />

