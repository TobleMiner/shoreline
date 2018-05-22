Shoreline
=========

A very fast pixelflut server with full IPv6 support written in C

# Compiling

## Dependencies

* SDL2
* libpthread
* libvncserver

Use ```make``` to build shoreline


# Usage

By default Shoreline runs in headless mode. In headless mode all user frontends are disabled. Use ```shoreline -f sdl``` to get a sdl window for drawing

There are a few more commandline switches:

```
-p <port>		Port to listen on (default 1234)
-b <address>		Address to listen on (default ::)
-w <width>		Width of drawing surface (default 1024)
-h <height>		Height of drawing surface (default 768)
-r <update rate>	Screen update rate in HZ (default 60)
-s <ring size>		Size of network ring buffer in bytes (default 65536)
-l <listen threads>	Number of threads used to listen for incoming connections (default 10)
-f <frontend>		Frontend to use as a display. May be specified multiple times. Use -f ? to list available frontends
```

## Resizing

While you can resize the shoreline output window while running doing so might significantly reduce the performance of the numa shoreline version until shoreline is restarted.
This bug exists because shoreline normally takes great care to allocate memory used by threads on memory sticks directly connected to the cores running the threads. But when
the window is resized the resize event is handled by the main thread. This leads to the main thread reallocating the framebuffers used by the threads, destroying the cpu node
locality of the buffers.

# Performance

Shoreline can easily handle full 10G line speed traffic on half way decent hardware (i7-6700, 32 GB dual channel DDR4 memory @2400 MHz)

On more beefy hardware (2x AMD EPYC 7821, 10x 8GB DDR4 ECC memory @2666 MHz, 6 memory channels) we are at about 37 Gbit/s

These results were obtained using [Sturmflut](https://github.com/TobleMiner/sturmflut) as a client
