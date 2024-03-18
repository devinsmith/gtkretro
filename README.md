# GTK

An ancient / retro version of GTK and supporting libraries for fun. This is
basically a fork of GTK 1.2.10 but modified a little bit to build on modern
systems using CMake.

The purpose is really to build GTK 1.2.x applications on modern systems and
not to target legacy distributions (after all, we're using CMake here). There
is no guarantee that this even builds on anything non-Posix and probably
requires a C99 (or higher) based compiler. It is only tested on Linux based
systems.

If you are using a distro that still packages GTK 1.2.10 you should continue
using that.

# Dependencies

You first need to build and install
[glibretro](https://github.com/devinsmith/glibretro) on your system.

In addition to that you need some X11 development packages installed:

```
sudo apt-get install libx11-dev libxi-dev
```

# Building

The current project uses CMake to build the library:

```
mkdir build
cd build
cmake ..
make
make install
```

# FAQs

* Should I use this instead of GTK2/GTK3/GTK4/GTK5?

No.

* Why maintain GTK1?

GTK1 is a very small code base and is easily maintainable by a single person,
especially since upstream is "dead".

