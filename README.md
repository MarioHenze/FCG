# Practical exercises *Foundations of Computer Graphics*

## Working on an exercise

Each exercise `<exercise>` lives in its own numbered folder and builds into two executables:

- **`<exercise>`** is your program. Implement the parts marked `TODO` in `<exercise>/handout/`, and leave everything
  else alone: `<exercise>/src/` holds the given code, most of which your program compiles as it is.
- **`<exercise>_solution`** is the reference implementation, built from `<exercise>/src/` without the gaps. Run it
  next to your own to compare, and read `<exercise>/src/` whenever you are stuck - the pieces you are asked to write
  are in there, which is deliberate.

The exercises are:

- **`0intro`**, a viewer for low- and high-dynamic-range images. It decodes an image, uploads it, and applies
  reciprocal-gamma correction on the CPU or GPU, with gamma 2.2 by default and adjustable per-channel controls in
  both modes. Its histogram panel is the part left for you: `binImage` in `0intro/handout/histogram.cpp` has to walk
  the image's pixels and count how often each code value occurs per channel. The panel works before you start, and
  says so: until your implementation runs, it reports that the histogram is unavailable, and the viewer keeps
  working.
- **`1barycentric`**, a triangle with one color per corner, drawn into a 16 by 16 framebuffer where each cell is one
  fragment. `barycentricWeights` and `shadeFragment` in `1barycentric/handout/barycentric.cpp` are the parts left for
  you: compute the barycentric weights of a fragment and blend the three corner colors with them. The window works
  before you start, and says so: until your implementation runs, it draws an empty framebuffer and reports what is
  still missing.

Build and run one of the two, for example with the `debug` preset:

```sh
cmake --preset debug
cmake --build --preset debug --target 0intro
./build/debug/bin/0intro
```

Both executables end up in the `bin/` directory of the preset's build tree, with one subdirectory per configuration
when using the multi-config `ninja` preset (`build-ninja/bin/Debug/0intro`).

The framework's API documentation is hosted at <https://brussig-tud.github.io/FCG-framework/>. For `0intro`, the
[Image library overview][image] and, in particular, the module on
[scoped native pixel access][pixels] are the relevant parts; for `1barycentric`, the
[Render library overview][render] and the [Core library overview][core] are. Both exercises use the applet, render
and extras libraries, whose overviews are linked from the same page.

[image]: https://brussig-tud.github.io/FCG-framework/fcg_image_overview.html
[pixels]: https://brussig-tud.github.io/FCG-framework/group__fcg__pixel__access.html
[render]: https://brussig-tud.github.io/FCG-framework/fcg_render_overview.html
[core]: https://brussig-tud.github.io/FCG-framework/fcg_core_overview.html


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
installed FCG package when available, and otherwise fetches the framework revision pinned in `CMakeLists.txt`. The
first configure therefore downloads the framework and its own dependencies, which takes a few minutes.

> **IMPORTANT NOTE** for ***GNOME* users**: Window decorations require *libdecor* development files to be installed. On Ubuntu:
> ```
> sudo apt install libdecor-0-dev
> ```
> Otherwise, the main window will have no title bar including minimize/maximize/close buttons.


## NOTE to maintainers

*FCG-Framework* installation rules default to disabled when embedded (via `FetchContent` or `CPM`), and enabled when
built standalone. Pass  `-DFCG_INCLUDE_PACKAGING=ON` to opt into framework installation from an embedded build.
`FCG_INSTALL_CMAKEDIR` overrides the package installation directory when packaging is enabled.

The exercise sources in `<exercise>/src` are the reference solution and the single source of truth. The student
version in `<exercise>/handout` is generated from them by `tools/elide.py`, which replaces each marked region with
the prompt the students get. Edit `<exercise>/src`, run `python3 tools/elide.py <exercise>/src <exercise>/handout`,
and commit both trees; CI checks that the scaffold is current. Both variants build, as `<exercise>` and
`<exercise>_solution`.

Point Git at the hooks in this repository once per clone, and a commit that touches the solution regenerates the
scaffold by itself:

```sh
git config core.hooksPath .githooks
```

The hook ignores commits that do not touch the solution, so it stays out of the way of anyone working in the scaffold.
It refuses a commit that touches the solution while that solution still has unstaged or untracked changes, because it
cannot tell which text such a commit would contain; stage those changes in the same commit, or bypass the check with
`git commit --no-verify`.

Exercises are frozen once they are handed out: students work in the scaffold, so regenerating an exercise that is
already in use would clash with their edits. The generator is meant for exercises still under development.

A new exercise needs its solution/scaffold pair added in two places: the `regenerate` call at the end of
`.githooks/pre-commit`, and the check step in `.github/workflows/ci.yml`.
