# EDMC Overlay for Linux
 
This is port of the idea of [EDMC Overlay] to the Linux. 

Only API is ported, everything else is made from scratch. This repo supports X11 only yet.

## New Features
* Added ability to show/hide overlay by 1 click in the main EDMC window, which can be used for the screenshots.
* Added basic "command" support compatible with Windows' version.
* Added ability to detect if EliteDangerous is top window (has input focus) and hide overlay if not.
* TTF fonts are supported.
* Now user can configure "normal"/"large" fonts' sizes from EDMC settings. Single config for all plugins and font-size per installed EDMC's plugin are supported.
* Added check if this plugin's folder is properly named `edmcoverlay` or `EDMCOverlay`. It will crash if not. Note, either of those 2 namings may break some other plugins. As authors of those use both. That should be addressed by broken plugins. Good loading code is present into EDMC-BioScan :
```
try:
    from EDMCOverlay import edmcoverlay
except ImportError:
    try:
        from edmcoverlay import edmcoverlay
    except ImportError:
        edmcoverlay = None
```
* Added WM_CLASS set to `edmc_linux_overlay_class` for the overlay window.
* Added Cairo to draw the shapes (if found installed in the system).

## Example Screenshot(s)

![ttf_example](https://github.com/alexzk1/edmcoverlay2/assets/4589845/60120533-ee49-4b47-9804-4cd3075d2426)

Bioscan's radar with Cairo:

![image](https://github.com/user-attachments/assets/2f673159-7cfb-4b0e-97d0-0eee7a60eddf)


## Installation

- Clone the repo into your EDMC plugins' directory
  - NB: you *must* name the directory `edmcoverlay`, not `edmcoverlay_for_linux`. You may clone repo elsewhere and symlink it as `edmcoverlay` to the plugins' directory. This is required because all other plugins use this name to access overlay (some are using `EDMCOverlay` naming too).
- Install the dependencies (mostly X11 development headers; on Ubuntu,
  the `xorg-dev` package may be sufficient), `cmake`.
- Run script `create_binary.sh` it will handle all needed.
- In the EDMC settings, configure the size and position of the overlay, default fonts' sizes, sizes per plugin.

## Dependencies

Full list of libraries used check into `cpp/CMakeLists.txt`. Those must be pre-installed in system before running compilation.

## Usage

EDMCOverlay for Linux aims to be 100% compatible with EDMC Overlay. 

Python library is a wrapper to pass json to the compiled binary.
Compiled binary can be used stand-alone for any other purposes as overlay. Binary listens on port 5010.


## Copyright

Copyright © 2020 Ash Holland. Licensed under the GPL (version 3 only).

Copyright © 2021-2024 Oleksiy Zakharov. Licensed under the GPL (version 3 only).

### Original references by Ash Holland

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
