# NOTE: Feel free to change the makefile to suit your own need.

# compile and link flags
CCFLAGS = -Wall -Og -g
LDFLAGS = -Wall -g

# make rules
TARGETS = rdt_sim 

all: $(TARGETS)

%.o: %.cc
	g++ $(CCFLAGS) -c -o $@ $<

rdt_sender.o: 	rdt_struct.h rdt_utils.h rdt_sender.h

rdt_receiver.o:	rdt_struct.h rdt_utils.h rdt_receiver.h 

rdt_sim.o:		rdt_struct.h rdt_utils.h

rdt_utils.o:	rdt_utils.h

rdt_sim: rdt_sim.o rdt_sender.o rdt_receiver.o rdt_utils.o
	g++ $(LDFLAGS) -o $@ $^

clean:
	rm -f *~ *.o $(TARGETS)
