#include "contiki.h"
#include "net/rime/rime.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "dev/button-sensor.h"
//#include "platform/sky/dev/temperature-sensor.h"
//#include "platform/sky/dev/light-sensor.h"
//#include "lib/sensors.h"
#include "dev/light-sensor.h"
//#include "dev/temperature-sensor.h"
#include "dev/sht11/sht11-sensor.h"


#include <stdio.h>

#define CHANNEL 135


struct example_neighbor {
  struct example_neighbor *next;
  linkaddr_t addr;
  struct ctimer ctimer;
  int hop_count_to_sink;
};

#define NEIGHBOR_TIMEOUT 60 * CLOCK_SECOND
#define MAX_NEIGHBORS 16
LIST(neighbor_table);
MEMB(neighbor_mem, struct example_neighbor, MAX_NEIGHBORS);
#define MAX_HOP_COUNT 9999
uint16_t my_hop_count_to_sink = MAX_HOP_COUNT;
static struct announcement example_announcement;

/*---------------------------------------------------------------------------*/
PROCESS(example_multihop_process, "multihop example");
AUTOSTART_PROCESSES(&example_multihop_process);
/*---------------------------------------------------------------------------*/
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

    printf("Got announcement from %d.%d, id %u, value %u\n",
      from->u8[0], from->u8[1], id, value);
    
    // Update my_hop_count_to_sink if this neighbor is closer to the sink
    if (value < my_hop_count_to_sink - 1) {
      uint16_t previous_my_hop_count_to_sink = my_hop_count_to_sink;
      my_hop_count_to_sink = value + 1;
      announcement_set_value(&example_announcement, my_hop_count_to_sink);
      printf("Update my hop count from %u to %u\n", previous_my_hop_count_to_sink, my_hop_count_to_sink);
      announcement_bump(&example_announcement);
    }

    for (e = list_head(neighbor_table); e != NULL; e = list_item_next(e)) {
      if (linkaddr_cmp(from, &e->addr)) {
        ctimer_set(&e->ctimer, NEIGHBOR_TIMEOUT, remove_neighbor, e);
        e->hop_count_to_sink = value;
        return;
      }
    }
    e = memb_alloc(&neighbor_mem);
    if (e != NULL) {
      linkaddr_copy(&e->addr, from);
      list_add(neighbor_table, e);
      ctimer_set(&e->ctimer, NEIGHBOR_TIMEOUT, remove_neighbor, e);
      e->hop_count_to_sink = value;
    }
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

  int min_hop_count = MAX_HOP_COUNT;
  num = list_length(neighbor_table);
  if (num > 0) {
    struct example_neighbor* pointer = list_head(neighbor_table);
    for (i = 0; i < num; ++i) {
      //printf("Check index %d neighbor %d.%d, its hop count is %u, compare to min_hop_count %u\n", 
      //          i, pointer->addr.u8[0], pointer->addr.u8[1], pointer->hop_count_to_sink, min_hop_count);
      if (pointer->hop_count_to_sink < min_hop_count) {
        n = pointer;
        min_hop_count = pointer->hop_count_to_sink;
      //  printf("this one has a smaller hop count %u compared to min_hop_count %u\n", pointer->hop_count_to_sink, min_hop_count);
      }
      pointer = list_item_next(pointer);
    }

    if (n != NULL) {
      printf("%d.%d: Forwarding packet to %d.%d (%d in list), hops %d\n", 
        linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
        n->addr.u8[0], n->addr.u8[1], num,
        packetbuf_attr(PACKETBUF_ATTR_HOPS));
      
      return &n->addr;
    }
  }
  
  printf("%d.%d: did not find a neighbor to forward to. Neighbor count: %d\n", 
    linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], num);
  return NULL;
}
static const struct multihop_callbacks multihop_call = {recv, forward};
static struct multihop_conn multihop;
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

  /* Activate the button sensor. We use the button to drive traffic -
     when the button is pressed, a packet is sent. */
  SENSORS_ACTIVATE(button_sensor);
  SENSORS_ACTIVATE(light_sensor);
  SENSORS_ACTIVATE(sht11_sensor);

  /* Loop forever, send a packet when the button is pressed. */
  while(1) {
    linkaddr_t to;

    /* Wait until we get a sensor event with the button sensor as data. */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event &&
			     data == &button_sensor);

    int light = light_sensor.value(0);
    int temperature = ((sht11_sensor.value(SHT11_SENSOR_TEMP) / 10) - 396) / 10;

    /* Copy the data string to the packet buffer. */
    int node_id = linkaddr_node_addr.u8[0] + linkaddr_node_addr.u8[1] * 256;
    char buffer[100];
    sprintf(buffer, "%d,%d,%d", node_id, light, temperature);
    packetbuf_copyfrom(buffer, strlen(buffer) + 1);

    /* Set the Rime address of the final receiver of the packet to
       1.0. This is a value that happens to work nicely in a Cooja
       simulation (because the default simulation setup creates one
       node with address 1.0). */
    to.u8[0] = 1;
    to.u8[1] = 0;

    /* Send the packet. */
    multihop_send(&multihop, &to);

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

