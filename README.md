# Practical exercises *Foundations of Computer Graphics*

blabla intro bla blub


## Building

To configure the exercises and the framework, run:

```sh
cmake -S . -B build-vscode -G Ninja
```

This requires CMake 3.31 or newer, Ninja, Git, a C/C++ compiler, and the framework's platform dependencies. CPM uses an
installed FCG package when available, otherwise it currently fetches the framework's `master` branch (will eventually
pin some specific tag or version).


## NOTE to maintainers

*FCG-Framework* installation rules default to disabled when embedded (via `FetchContent` or `CPM`), and enabled when
built standalone. Pass  `-DFCG_INCLUDE_PACKAGING=ON` to opt into framework installation from an embedded build.
`FCG_INSTALL_CMAKEDIR` overrides the package installation directory when packaging is enabled.
