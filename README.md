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

# Performance

Shoreline can easily handle full 10G line speed traffic on half way decent hardware (i7-6700, 32 GB dual channel DDR4 memory @2400 MHz)

On more beefy hardware (2x AMD EPYC 7821, 10x 8GB DDR4 ECC memory @2666 MHz, 6 memory channels) we are at about 37 Gbit/s

These results were obtained using [Sturmflut](https://github.com/TobleMiner/sturmflut) as a client
