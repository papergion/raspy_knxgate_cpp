#!make
# This make needs:
# g++
#

include ../MakeHelper

#The Directories, Source, Includes, Objects, Binary and Resources
BUILDDIR	?= $(CURDIR)/obj/$(BUILD)
TARGETDIR	?= $(CURDIR)/bin/$(BUILD)

#Compiler and Linker
BUILD	?= release
BITS	?= 64
CC		:= g++

#Flags, Libraries and Includes
#CFLAGS.common	:= -std=c++17 -m$(BITS) -Wall -Wextra
CFLAGS.common   := -std=c++17 -Wall -Wextra
CFLAGS.debug 	:= $(CFLAGS.common) -g
CFLAGS.release	:= $(CFLAGS.common) -Werror -O3
CFLAGS			?= $(CFLAGS.$(BUILD))
LIB		:= -L$(TARGETDIR) -leasysocket -lpthread -Wl,-rpath='$${ORIGIN}'
INC		:= -I$(CURDIR)/../easysocket/include

build:
	(cd ../easysocket && make TARGETDIR="$(TARGETDIR)")
	$(CC) $(CFLAGS) $(INC) knxtry.c  -o $(TARGETDIR)/$(call MakeExe,knxtry) $(LIB)
	$(CC) $(CFLAGS) $(INC) knxlog.c  -o $(TARGETDIR)/$(call MakeExe,knxlog) $(LIB)
	$(CC) $(CFLAGS) $(INC) knxtcp.c  -o $(TARGETDIR)/$(call MakeExe,knxtcp) $(LIB)
	$(CC) $(CFLAGS) $(INC) knxmonitor.c  -o $(TARGETDIR)/$(call MakeExe,knxmonitor) $(LIB)
	$(CC) $(CFLAGS) $(INC) knxfirmware.c  -o $(TARGETDIR)/$(call MakeExe,knxfirmware) $(LIB)
	$(CC) $(CFLAGS) $(INC) knxgate_x.c knx_mqtt.c knx_hue.c -lpaho-mqtt3c -o $(TARGETDIR)/$(call MakeExe,knxgate_x) $(LIB)
	$(CC) $(CFLAGS) $(INC) knxdiscover.c knx_mqtt_disc.c -lpaho-mqtt3c -o $(TARGETDIR)/$(call MakeExe,knxdiscover) $(LIB)

template:
	$(call MD,$(TARGETDIR))
	$(call MD,$(BUILDDIR))

clean:
	$(call RM,$(BUILDDIR))

cleaner: clean
	$(call RM,$(TARGETDIR))

clean-deps: clean
	(cd ../easysocket && make clean)

cleaner-deps: cleaner
	(cd ../easysocket && make cleaner)

.PHONY: build template clean cleaner clean-deps cleaner-deps
