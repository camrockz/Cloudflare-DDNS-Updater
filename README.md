# Cloudflare DDNS Updater

A simple executable to check the IP address that points to the current machine, and update Cloudflare's DDNS system with our domain name.

## Requirements

- C99 libc and compiler
- Libcurl
- Libjson-c
- CMake 3.0+ (optional)

## Building

```
git clone https://github.com/camrockz/Cloudflare-DDNS-Updater.git
mkdir build
cd /build
```
If you are using CMake

```
cmake .. -DCMAKE_BUILD_TYPE=release
make
```
Else use GCC or Clang

```
gcc -O3 -DNDEBUG -o ipup ../stable.c -lcurl -ljson-c
```
## Running

Rename example-config.json to config.json and replace the strings inside the double quotes **" "** with those that apply to your Cloudflare account. Your key can be found in your Cloudflare account settings.

Run as a cron job every 60 seconds.

```
* * * * * cd /path/to/build/directory/ && ./ipup
```
