#include "contiki.h"
#include "net/rime/rime.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "dev/button-sensor.h"
#include "dev/light-sensor.h"
#include "dev/sht11/sht11-sensor.h"


#include <stdio.h>

#define CHANNEL 135

// Neighbor data struct
struct example_neighbor {
  struct example_neighbor *next;
  linkaddr_t addr;
  struct ctimer ctimer;
  int hop_count_to_sink;
};

// Neighbor table
#define NEIGHBOR_TIMEOUT 60 * CLOCK_SECOND
#define MAX_NEIGHBORS 16
LIST(neighbor_table);
MEMB(neighbor_mem, struct example_neighbor, MAX_NEIGHBORS);

// Hop count
#define MAX_HOP_COUNT 9999
static uint16_t my_hop_count_to_sink = MAX_HOP_COUNT;

// Announcement
static struct announcement example_announcement;

// Send announcements periodically
#define ANNOUNCEMENT_INTERVAL 30 * CLOCK_SECOND
static struct ctimer announcement_timer;
static void handle_announcement_timer(void* ptr)
{
  announcement_bump(&example_announcement);
  ctimer_reset(&announcement_timer);
}

// Send data to the sink node every minute
#define SEND_DATA_INTERVAL 60 * CLOCK_SECOND
static int light = 0;
static int temperature = 0;
static struct ctimer send_data_timer;
static int temperature_measure_counter = 0;


/*---------------------------------------------------------------------------*/
PROCESS(example_multihop_process, "multihop example");
AUTOSTART_PROCESSES(&example_multihop_process);
/*---------------------------------------------------------------------------*/

// Convert linkaddr_t to node ID
static int addr_to_node_id(linkaddr_t addr)
{
  return addr.u8[0] + addr.u8[1] * 256;
}

// Update the hop count from this node to the sink node
static void update_my_hop_count_to_sink()
{
  int is_hop_count_updated = 0;
  uint16_t my_previous_hop_count = my_hop_count_to_sink;

  int min_neighbor_hop = MAX_HOP_COUNT;
  struct example_neighbor *e;
  //printf("Neighbor hop counts: ");
  for (e = list_head(neighbor_table); e != NULL; e = list_item_next(e)) {
    //printf("nodeId %d hop %u / ", addr_to_node_id(e->addr), e->hop_count_to_sink);
    if (e->hop_count_to_sink < min_neighbor_hop) {
      min_neighbor_hop = e->hop_count_to_sink;
    }
  }
  //printf("\n");

  // My hop count changed(possibly increase or decrease)
  if (min_neighbor_hop != MAX_HOP_COUNT && my_hop_count_to_sink != min_neighbor_hop + 1) {
      my_hop_count_to_sink = min_neighbor_hop + 1;
      is_hop_count_updated = 1;
  }
  
  // If my hop count is updated, need to notify my neighbors
  if (is_hop_count_updated) {
      announcement_set_value(&example_announcement, my_hop_count_to_sink);
      printf("Update my hop count from %u to %u\n", my_previous_hop_count, my_hop_count_to_sink);

      announcement_bump(&example_announcement);
  }
}

/*
 * This function is called by the ctimer present in each neighbor
 * table entry. The function removes the neighbor from the table
 * because it has become too old.
 */
static void
remove_neighbor(void *n)
{
  struct example_neighbor *e = n;

  list_remove(neighbor_table, e);
  memb_free(&neighbor_mem, e);

  // After removing a neighbor, update my hop count and announce it to my rest neighbors
  update_my_hop_count_to_sink();
}

/*---------------------------------------------------------------------------*/
/*
 * This function is called when an incoming announcement arrives. The
 * function checks the neighbor table to see if the neighbor is
 * already present in the list. If the neighbor is not present in the
 * list, a new neighbor table entry is allocated and is added to the
 * neighbor table.
 */
static void
received_announcement(struct announcement *a,
                      const linkaddr_t *from,
		      uint16_t id, uint16_t value)
{
  struct example_neighbor *e;

    //printf("Got announcement from nodeId %d, id %u, value %u\n",
    //  addr_to_node_id(*from), id, value);

    // If the neighbor already exist in the table, reset the expire timer
    for (e = list_head(neighbor_table); e != NULL; e = list_item_next(e)) {
      if (linkaddr_cmp(from, &e->addr)) {
        ctimer_set(&e->ctimer, NEIGHBOR_TIMEOUT, remove_neighbor, e);
        e->hop_count_to_sink = value;
        return;
      }
    }

    // If the neighbor doesn't exist in the table, create a new neighbor and add it to the table 
    e = memb_alloc(&neighbor_mem);
    if (e != NULL) {
      linkaddr_copy(&e->addr, from);
      list_add(neighbor_table, e);
      ctimer_set(&e->ctimer, NEIGHBOR_TIMEOUT, remove_neighbor, e);
      e->hop_count_to_sink = value;
    }

    // After being announced by a neighbor, update my hop count and announce it to my rest neighbors
    update_my_hop_count_to_sink();
}

/*---------------------------------------------------------------------------*/
/*
 * This function is called at the final recepient of the message.
 */
static void
recv(struct multihop_conn *c, const linkaddr_t *sender,
     const linkaddr_t *prevhop,
     uint8_t hops)
{
  printf("multihop message received '%s'\n", (char *)packetbuf_dataptr());
}

/*
 * This function is called to forward a packet. The function picks a
 * random neighbor from the neighbor list and returns its address. The
 * multihop layer sends the packet to this address. If no neighbor is
 * found, the function returns NULL to signal to the multihop layer
 * that the packet should be dropped.
 */
static linkaddr_t *
forward(struct multihop_conn *c,
	const linkaddr_t *originator, const linkaddr_t *dest,
	const linkaddr_t *prevhop, uint8_t hops)
{
  int num, i;
  struct example_neighbor *n = NULL;

  // Find the neighbor in the neighbor table with the minimum hop count, then forward to it
  int min_hop_count = MAX_HOP_COUNT;
  num = list_length(neighbor_table);
  if (num > 0) {
    struct example_neighbor* pointer = list_head(neighbor_table);
    for (i = 0; i < num; ++i) {
      //printf("Check index %d neighbor nodeId %d, its hop count is %u, compare to min_hop_count %u\n", 
      //         i, addr_to_node_id(pointer->addr), pointer->hop_count_to_sink, min_hop_count);

      //printf("originator: %d, prevhop: %d, pointer: %d, dest: %d\n", addr_to_node_id(*originator), addr_to_node_id(*prevhop), addr_to_node_id(pointer->addr), addr_to_node_id(*dest));

      int nodeId = addr_to_node_id(pointer->addr);
      // If the destination is a neighbor, forward to it directly
      if (nodeId == addr_to_node_id(*dest)) {
        n = pointer;
        break;
      }

      // Don't forward to the previous node or the original node
      if (nodeId != addr_to_node_id(*originator) && nodeId != addr_to_node_id(*prevhop)) {

        // Find the neighbor node that has the minimum hop count
        if (pointer->hop_count_to_sink < min_hop_count) {
          n = pointer;
          min_hop_count = pointer->hop_count_to_sink;
          //printf("this one has a smaller hop count %u compared to min_hop_count %u\n", pointer->hop_count_to_sink, min_hop_count);
        }
      }
      pointer = list_item_next(pointer);
    }

    // Forward to the neighbor with the minimum hop count
    if (n != NULL) {
      printf("My nodeId %d: Forwarding packet to nodeId %d (%d in list), hops %d\n", 
        addr_to_node_id(linkaddr_node_addr),
        addr_to_node_id(n->addr), num,
        packetbuf_attr(PACKETBUF_ATTR_HOPS));
      
      return &n->addr;
    }
  }
  
  printf("My nodeId %d: did not find a neighbor to forward to. Neighbor count: %d\n", 
    addr_to_node_id(linkaddr_node_addr), num);
  return NULL;
}

static const struct multihop_callbacks multihop_call = {recv, forward};
static struct multihop_conn multihop;

// Send packet data to the sink node, this function should be run every a minute
static void send_data_to_sink()
{
  // Measure light every minite (SEND_DATA_INTERVAL)
  light = light_sensor.value(0);

  // Measure temperature every 5 minutes (SEND_DATA_INTERVAL * 5)
  --temperature_measure_counter;
  if (temperature_measure_counter <= 0) {
    temperature = ((sht11_sensor.value(SHT11_SENSOR_TEMP) / 10) - 396) / 10;
    temperature_measure_counter = 5;
    printf("measure temperature this time!!!\n");
  }

  /* Copy the data string to the packet buffer. */
  int node_id = addr_to_node_id(linkaddr_node_addr);
  char buffer[100];
  sprintf(buffer, "%d,%d,%d", node_id, light, temperature);
  packetbuf_copyfrom(buffer, strlen(buffer) + 1);

  /* Set the Rime address of the final receiver of the packet to
    1.0. This is a value that happens to work nicely in a Cooja
    simulation (because the default simulation setup creates one
    node with address 1.0). */
  linkaddr_t to;
  to.u8[0] = 1;
  to.u8[1] = 0;

  /* Send the packet. */
  printf("Send packet from %d to %d...\n", node_id, addr_to_node_id(to));
  multihop_send(&multihop, &to);
}

static void handle_send_data_timer(void* ptr)
{
  int random_delay_time = random_rand() % 10 * CLOCK_SECOND;
  static struct ctimer delay_send_timer;
  ctimer_set(&delay_send_timer, random_delay_time, send_data_to_sink, NULL);

  ctimer_reset(&send_data_timer);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_multihop_process, ev, data)
{
  PROCESS_EXITHANDLER(multihop_close(&multihop);)
    
  PROCESS_BEGIN();

  /* Initialize the memory for the neighbor table entries. */
  memb_init(&neighbor_mem);

  /* Initialize the list used for the neighbor table. */
  list_init(neighbor_table);

  /* Open a multihop connection on Rime channel CHANNEL. */
  multihop_open(&multihop, CHANNEL, &multihop_call);

  /* Register an announcement with the same announcement ID as the
     Rime channel we use to open the multihop connection above. */
  announcement_register(&example_announcement,
			CHANNEL,
			received_announcement);

  // Send my_hop_count_to_sink as the value of announcement
  announcement_set_value(&example_announcement, my_hop_count_to_sink);

  SENSORS_ACTIVATE(button_sensor);
  SENSORS_ACTIVATE(light_sensor);
  SENSORS_ACTIVATE(sht11_sensor);

  // Announce to neighbors periodically
  ctimer_set(&announcement_timer, ANNOUNCEMENT_INTERVAL, handle_announcement_timer, NULL);

  // Send data to sink node periodically
  ctimer_set(&send_data_timer, SEND_DATA_INTERVAL, handle_send_data_timer, NULL);
  
  /* Loop forever, send a packet when the button is pressed. */
  while(1) {

    // /* Wait until we get a sensor event with the button sensor as data. */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event &&
			     data == &button_sensor);

    send_data_to_sink();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

