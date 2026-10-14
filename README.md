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

The exercise sources in `0intro/src` are the reference solution and the single source of truth. The student version
in `0intro/handout` is generated from them by `tools/elide.py`, which replaces each marked region with the prompt the
students get. Edit `0intro/src`, run `python3 tools/elide.py 0intro/src 0intro/handout`, and commit both trees; CI
checks that the scaffold is current. Both variants build, as `0intro` and `0intro_solution`.

Point Git at the hooks in this repository once per clone, and a commit that touches the solution regenerates the
scaffold by itself:

```sh
git config core.hooksPath .githooks
```

The hook ignores commits that do not touch the solution, so it stays out of the way of anyone working in the scaffold.
It refuses a commit that touches the solution while that solution still has unstaged or untracked changes, because it
cannot tell which text such a commit would contain; stage those changes in the same commit, or bypass the check with
`git commit --no-verify`.
