CC = gcc

CCFLAGS = -g -Wfatal-errors -w -O2 `pkg-config --cflags --libs gtk+-2.0` -I/usr/include/glib-2.0
LDLIBS = -lglib-2.0 -lhiredis -levent -lprotobuf-c
PROTO = record_alarm.proto record_cdb.proto record_ha.proto

SRC_C = test.c db_api.c vnf_timer.c db_api_sync.c testsync.c scheduler.c record_table.c db_api_log.c db_api_common.c

PB_FILE_C = ${patsubst %.proto, %.pb-c.c, $(PROTO)}
OBJS = ${patsubst %.c, %.o, $(All_SRC_C)}


All_SRC_C = $(SRC_C) $(PB_FILE_C)


$(PB_FILE_C) : $(PROTO)
	protoc-c --c_out=. $^

$(OBJS) : $(PB_FILE_C) $(SRC_C)
	$(CC) $(CCFLAGS) -c $^


.DEFAULT_GOAL := DbTestApi

DbTestApi: $(OBJS)
	$(CC) $^ -o $@ $(LDLIBS)



.PHONY: clean
clean:
	rm -rf DbTestApi *.o *.pb-c.c


