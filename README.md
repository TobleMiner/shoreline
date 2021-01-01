Shoreline
=========

A very fast pixelflut server with full IPv6 support written in C

# Compiling

## Dependencies

* SDL2
* libpthread
* libvncserver
* libnuma (numactl)
* libfreetype2

On \*buntu/Debian distros use `sudo apt install git build-essential libsdl2-dev libpthread-stubs0-dev libvncserver-dev libnuma-dev libfreetype6-dev` to install the dependencies.

Use ```make``` to build shoreline

Optionally the following environment variables can be set to control how shoreline is built:

* INCLUDE_DIR: Allows to select an include dir other than /usr/include
* OPTFLAGS: Allows to set the optimization flags used for this build
* FEATURES: Allows to select what features shoreline will be built with (see Makefile for available features)

# Usage

By default Shoreline runs in headless mode. In headless mode all user frontends are disabled. Use ```shoreline -f sdl``` to get a sdl window for drawing

There are a few more commandline switches:

```
Options:
  -p <port>                        Port to listen on (default 1234)
  -b <address>                     Address to listen on (default ::)
  -w <width>                       Width of drawing surface (default 1024)
  -h <height>                      Height of drawing surface (default 768)
  -r <update rate>                 Screen update rate in HZ (default 60)
  -s <ring size>                   Size of network ring buffer in bytes (default 65536)
  -l <listen threads>              Number of threads used to listen for incoming connections (default 10)
  -f <frontend,[option=value,...]> Frontend to use as a display. May be specified multiple times. Use -f ? to list available frontends and options
  -t <fontfile>                    Enable fancy text rendering using TTF, OTF or CFF font from <fontfile>
  -d <description>                 Set description text to be displayed in upper left corner (default https://github.com/TobleMiner/shoreline)
  -?                               Show this help
```

## Frontend options

When specifying a frontend frontend-specific options may be passed to the frontend. For example the VNC frontend can be configured
to use a nonstandard port:

`shoreline -f vnc,port=2342`

All available frontends and their options can be listed using `shoreline -f ?`.

## Supported Pixelflut commands

```
PX <x> <y> <rrggbb|rrggbbaa> # Set pixel @(x,y) to specified hex color
PX <x> <y>                   # Get pixel @(x,y) as hex
SIZE                         # Get size of drawing surface
OFFSET <x> <y>               # Apply offset (x,y) to all further pixel draws on this connection
```

## Alpha blending

By default alpha blending is disabled. This might cause images that contain transparency to be displayed incorrectly. You can enable alpha blending
at compile time by adding `ALPHA_BLENDING` to the `FEATURES` environment variable. However, be warned this will have a large impact on performance
since each pixel drawn turns into a memory read, modify, write instead of just a memory write.

## Statistics

To enable on-screen statistics display shoreline needs to be passed a TTF (other formats supported by libfreetype2 will work, too) font via the `-t` option.

When running a system with a graphical frontend chances are you do have some TTF fonts in `/usr/share/fonts/TTF/`.

### Statistics API

There is a special frontend called `statistics`. When enabled it serves a simple TCP based statistics API (default port 1235). Upon
connecting to the statistics API it dumps a JSON object and closes the connection.

This is an example JSON object returned by the API:

```
{
  "traffic": {
    "bytes": 7292518314,
    "pixels": 405139493
  },
  "throughput": {
    "bytes": 512289334,
    "pixels": 28460518
  },
  "connections": 10,
  "fps": 59
}
```

## Container setup

The awesome Sebastian Bernauer built a Docker based environment that makes
running shoreline with fancy monitoring and colorful statistics a breeze:
[pixelflut-infrastructure](https://github.com/sbernauer/pixelflut-infrastructure)

For optimum performance shoreline should be run on a host separate from the Docker
host. Trying to run shoreline inside a Docker container might impact performance
severely depending on your hardware and Docker configuration.

# Troubleshooting

When using shoreline with a lot of clients default resource limits applied to processes may not be sufficient.

## sysctl

The default maximum number of allowed vm mappings may not be enough. Consider increasing it by setting

```
vm.max_map_count = 1000000
```

in `/etc/sysctl.d/50-vm.conf` and reloading it using `sysctl --system`.

## security/limits

Shoreline can hit the maximum number of allowed file descriptors quite easily. Increase it by setting

```
*		soft 	nofile		262144
*		hard 	nofile		262144
```
in `/etc/security/limits.conf`.

## systemd

When running shoreline through a systemd service you will need to increase the default cgroup limits for larger installations.
Consider adding the following to the `[Service]` section of your shoreline unit file.

```
TasksMax=infinity
LimitSTACK=infinity
LimitNOFILE=infinity
LimitNPROC=infinity
```

## Hardware vulnerability mitigations

Over the past years an ever-growing set of mitigations for hardware vulnerabilities have been added to Linux. If you are only
running trusted code on your system you might want to add `mitigations=off` to your kernel cmdline. This can result in a
considerable performance increase. On Debian this change can by persisted by adding `mitigations=off` to GRUB_CMDLINE_LINUX in
`/etc/default/grub` and running `update-grub`.

## Reliability

Misbehaving VNC clients can occasionally trigger assertions in the VNC server library. When running shoreline with the VNC frontend
exposed to arbitrary clients, adding a VNC multiplexer like [VNCmux](https://github.com/TobleMiner/vncmux/) can improve reliability of
shoreline considerably.

# IP-Based Rate-Limiting

In order to rate-limit every IP to 10 established connections, you can simply use iptables:

`iptables -A INPUT -p tcp -m tcp --dport 1234 --tcp-flags FIN,SYN,RST,ACK SYN -m connlimit --connlimit-above 10 --connlimit-mask 32 --connlimit-saddr -j REJECT --reject-with icmp-port-unreachable`
`ip6tables -A INPUT -p tcp -m tcp --dport 1234 --tcp-flags FIN,SYN,RST,ACK SYN -m connlimit --connlimit-above 10 --connlimit-mask 128 --connlimit-saddr -j REJECT --reject-with icmp6-port-unreachable`

# Performance

Shoreline can easily handle full 10G line speed traffic on half way decent hardware (i7-6700, 32 GB dual channel DDR4 memory @2400 MHz)

On more beefy hardware (2x AMD EPYC 7821, 10x 8GB DDR4 ECC memory @2666 MHz, 6 memory channels) we are at about 37 Gbit/s

These results were obtained using [Sturmflut](https://github.com/TobleMiner/sturmflut) as a client

## VNC

With many VNC clients performance can degrade. Running a VNC multiplexer like [VNCmux](https://github.com/TobleMiner/vncmux/), even on the same host,
can improve performance drastically.
