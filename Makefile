CONTIKI = ../..

TARGET = sky

all: sensor_node sink_node

CONTIKI_WITH_RIME=1

include $(CONTIKI)/Makefile.include
