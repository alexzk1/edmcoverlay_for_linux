# edmcoverlay2

[EDMC Overlay][] for Linux.

## Installation

- Clone the repo into your EDMC plugins directory
  - NB: you *must* name the directory `edmcoverlay`, not `edmcoverlay2`
- Install the dependencies (mostly X11 development headers; on Ubuntu,
  the `xorg-dev` package may be sufficient)
- Run `cmake . & make`
- In the EDMC settings, configure the size and position of the overlay

## Usage

edmcoverlay2 aims to be 100% compatible with EDMC Overlay. Python library is a wrapper to pass json to the compiled binary.
Compiled binary can be used stand-alone for any other purposes as overlay. Binary listens on port 5010.
TTF fonts are supported.

![ttf_example](https://github.com/alexzk1/edmcoverlay2/assets/4589845/60120533-ee49-4b47-9804-4cd3075d2426)

Some features are not yet implemented, and there are likely to be bugs.

## Contributing

Everyone interacting with this project is expected to abide by the terms
of the Contributor Covenant Code of Conduct. Violations should be
reported to coc-enforcement-edmcoverlay2@sorrel.sh.

## Copyright

Copyright © 2020 Ash Holland. Licensed under the GPL (version 3 only).
Copyright © 2021-2024 Oleksiy Zakharov. Licensed under the GPL (version 3 only).

edmcoverlay2 is heavily based on [X11 overlay][] by @ericek111 (GPLv3).

Additionally, parts of edmcoverlay2 are copied from other projects:

- [gason][] (MIT)
- [lib_netsockets][] (Apache 2.0)

edmcoverlay2 would not exist without them.

Copyright notices can be found in the relevant source files.

[EDMC Overlay]: https://github.com/inorton/EDMCOverlay
[gason]: https://github.com/vivkin/gason
[lib_netsockets]: https://github.com/pedro-vicente/lib_netsockets
[X11 overlay]: https://gist.github.com/ericek111/774a1661be69387de846f5f5a5977a46
