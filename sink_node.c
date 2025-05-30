#include "contiki.h"
#include "net/rime/rime.h"

#include <stdio.h>

#define CHANNEL 135

/*---------------------------------------------------------------------------*/
PROCESS(example_multihop_process, "multihop example");
AUTOSTART_PROCESSES(&example_multihop_process);
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*
 * This function is called when an incoming announcement arrives
 */
static void
received_announcement(struct announcement *a,
                      const linkaddr_t *from,
		      uint16_t id, uint16_t value)
{
  // It's the sink node, no need to maintain the neighbor table
}

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

/*---------------------------------------------------------------------------*/
/*
 * This function is called at the final recepient of the message.
 */
static void
recv(struct multihop_conn *c, const linkaddr_t *sender,
     const linkaddr_t *prevhop,
     uint8_t hops)
{
  // Output the received data in a format like:
  // nodeID                 light                   temperature 
  // 127                    180                     25
  char* recv_data = (char *)packetbuf_dataptr();
  
  char data_list[3][20];
  char* token = strtok(recv_data, ",");
  int i = 0;
  while (token != NULL) {
    sprintf(data_list[i], token);
    token = strtok(NULL, ",");
    ++i;
  }

  if (i == 2) {
    printf("%-10s %-10s\n%-10s %-10s\n", "nodeID", "light", data_list[0], data_list[1]);
  }
  else if (i == 3) {
    printf("%-10s %-10s %-10s\n%-10s %-10s %-10s\n", "nodeID", "light", "temperature", data_list[0], data_list[1], data_list[2]);
  }
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
  // It's the sink node, no need to forward
  return NULL;
}

static const struct multihop_callbacks multihop_call = {recv, forward};
static struct multihop_conn multihop;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_multihop_process, ev, data)
{
  PROCESS_EXITHANDLER(multihop_close(&multihop);)
    
  PROCESS_BEGIN();

  /* Open a multihop connection on Rime channel CHANNEL. */
  multihop_open(&multihop, CHANNEL, &multihop_call);

  /* Register an announcement with the same announcement ID as the
     Rime channel we use to open the multihop connection above. */
  announcement_register(&example_announcement,
			CHANNEL,
			received_announcement);

  /* Set a dummy value to start sending out announcments. */
  announcement_set_value(&example_announcement, 0);

  // Announce to neighbors periodically
  ctimer_set(&announcement_timer, ANNOUNCEMENT_INTERVAL, handle_announcement_timer, NULL);

  /* Loop forever, send a packet when the button is pressed. */
  while(1) {
    /* Wait any events. */
    PROCESS_WAIT_EVENT();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

