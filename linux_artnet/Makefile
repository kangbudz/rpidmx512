DEFINES =NODE_ARTNET LIGHTSET_PORTS=4
DEFINES+=ARTNET_HAVE_FAILSAFE_RECORD
DEFINES+=RDM_RESPONDER 

DEFINES+=OUTPUT_DMX_MONITOR

#DEFINES+=NDEBUG

SRCDIR=src lib

LIBS=

include ../firmware-template-linux/Rules.mk

prerequisites:
	./generate_sofware_version_id.sh
