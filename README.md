# Cloudflare DDNS Updater

A simple executable to check the IP address that points to the current machine, and update Cloudflare's DDNS system with our domain name.

## Requirements

- GCC or Clang
- Libcurl
- Libjson-c
- CMake 3.0+
- Systemd (optional)

## Building

```
git clone https://github.com/camrockz/Cloudflare-DDNS-Updater.git
mkdir build
cd /build
```
###With systemd support

```
cmake .. -DCMAKE_BUILD_TYPE=release -DSYSTEMD=ON
make
```
###Without systemd support

```
cmake .. -DCMAKE_BUILD_TYPE=release -DSYSTEMD=OFF
make
```
## Install and Run

In a root user shell

```
cp ipup /usr/bin/
mkdir /etc/ipup
cp ../ipup.conf /etc/ipup/
chown root:root /usr/bin/ipup /etc/ipup/ipup.conf
```
Now edit /etc/ipup/ipup.conf with your cloudflare settings

For systemd use

```
cp ../ipup.service /usr/lib/systemd/system/
systemctl enable ipup
systemctl start ipup
```

For init systems other than systemd, install a startup script for the ipup executable as per instructions for your init system.
