Shoreline
=========

A very fast pixelflut server with full IPv6 support written in C

# Compiling

## Dependencies

* SDL2
* libpthread

Use ```make``` to build shoreline


# Usage

Shoreline provides a SDL window for drawing. There are a few commandline switches:

```
-p <port>		Port to listen on (default 1234)
-b <address>		Address to listen on (default ::)
-w <width>		Width of drawing surface (default 1024)
-h <height>		Height of drawing surface (default 768)
-r <update rate>	Screen update rate in HZ (default 60)
-s <ring size>		Size of network ring buffer in bytes (default 65536)
-l <listen threads>	Number of threads used to listen for incoming connections (default 10)
```

# Performance

Shoreline can easily handle full 10G line speed traffic on half way decent hardware (i7-6700, 32 GB dual channel DDR4 memory @2400 MHz)

This result was obtained using [Sturmflut](https://github.com/TobleMiner/sturmflut) as a client
