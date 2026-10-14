# Practical exercises *Foundations of Computer Graphics*

blabla intro bla blub


## Building

The repository ships a `CMakePresets.json` with a static `debug` and `release` configuration.
To configure and build the exercises, run:

```sh
cmake --preset debug
cmake --build --preset debug
```

The build tree ends up in `build/debug`; use the `release` preset for an optimized build.

> **NOTE** for users of IDEs with a configuration switcher (Visual Studio, VS Code): the `ninja` preset builds with *Ninja Multi-Config* into a single `build-ninja` tree that holds both configurations side by side, which is what the configuration switcher GUI of these IDEs handles best, and *Ninja* builds considerably faster than MSBuild or make. Configure with `cmake --preset ninja`, then pick a configuration with `cmake --build --preset ninja-debug` (or `ninja-release`). This only needs `ninja` on `PATH`; the plain `debug` and `release` presets keep working without it.

This requires CMake 3.31 or newer, Git, a C/C++ compiler, and the framework's platform dependencies. CPM uses an
installed FCG package when available, otherwise it currently fetches the framework's `master` branch (will eventually
pin some specific tag or version).

> **IMPORTANT NOTE** for ***GNOME* users**: Window decorations require *libdecor* development files to be installed. On Ubuntu:
> ```
> sudo apt install libdecor-0-dev
> ```
> Otherwise, the main window will have no title bar including minimize/maximize/close buttons.


## NOTE to maintainers

*FCG-Framework* installation rules default to disabled when embedded (via `FetchContent` or `CPM`), and enabled when
built standalone. Pass  `-DFCG_INCLUDE_PACKAGING=ON` to opt into framework installation from an embedded build.
`FCG_INSTALL_CMAKEDIR` overrides the package installation directory when packaging is enabled.
