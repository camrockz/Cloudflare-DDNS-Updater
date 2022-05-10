BUILD = build
DEBUG = OFF

default: daemon.o
	cc $(BUILD)/daemon.o -lcurl -ljson-c -o $(BUILD)/ipup

systemd: systemd.o
	cc $(BUILD)/systemd.o -lcurl -ljson-c -lsystemd -o $(BUILD)/ipup

daemon.o: mkdir
ifeq ($(DEBUG), ON)
	cc -c daemon.c -g -o $(BUILD)/daemon.o
else
	cc -c daemon.c -o $(BUILD)/daemon.o
endif

systemd.o: mkdir
ifeq ($(DEBUG), ON)
	cc -c daemon.c -g -DSYSTEMD -o $(BUILD)/systemd.o
else
	cc -c daemon.c -DSYSTEMD -o $(BUILD)/systemd.o
endif

mkdir:
	mkdir -p $(BUILD)
clean:
	rm -rf $(BUILD)

install-systemd: install
	install -o root -m 755 ipup.service /usr/lib/systemd/system/

install:
	install -d /etc/ipup/
	install -o root -m 755 ipup.conf /etc/ipup/
	install -o root -m 755 $(BUILD)/ipup /usr/bin/