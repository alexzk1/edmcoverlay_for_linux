# EDMC Overlay for Linux
 
This is port of the idea of [EDMC Overlay] to the Linux. 

Only API is ported, everything else is made from scratch. This repo supports X11 only yet.

## New Features
* Added ability to show/hide overlay by 1 click in the main EDMC window, which can be used for the screenshots.
* Added basic "command" support compatible with Windows' version.
* Added ability to detect if EliteDangerous is top window (has input focus) and hide overlay if not.
* TTF fonts are supported.
* Added ability to draw SVG images on overlay.
* Now user can configure "normal"/"large" fonts' sizes from EDMC settings. Single config for all plugins and font-size per installed EDMC's plugin are supported.
* Now TCP connection on port 5010 is kept alive as on Windows version (server do not drop connection).
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
* ~~Added Cairo to draw the shapes (if found installed in the system).~~ Everything is converted into SVG and than `lunasvg` is used to render.
* Added multiline support. Now binary replaces '\t' with fixed amount of the spaces and properly handles '\n' accounting current font used. Python object got method `is_multiline_supported()`. It can be tested by other plugins as:
```
def supports_multiline(obj) -> bool:
    return (
        hasattr(obj, "is_multiline_supported")
        and callable(getattr(obj, "is_multiline_supported"))
        and obj.is_multiline_supported()
    )
```
* Added emoji render too.
There is method `is_emoji_supported()`, similary you can check if you can send emoji to overlay:
```
def supports_emoji(obj) -> bool:
    return (
        hasattr(obj, "is_emoji_supported")
        and callable(getattr(obj, "is_emoji_supported"))
        and obj.is_emoji_supported()
    )

```
## Example Screenshot(s)

![ttf_example](https://github.com/alexzk1/edmcoverlay2/assets/4589845/60120533-ee49-4b47-9804-4cd3075d2426)

Bioscan's radar with Cairo:

![image](https://github.com/user-attachments/assets/2f673159-7cfb-4b0e-97d0-0eee7a60eddf)


## SVG Images
You can send SVG picture using overlay's method:
```
overlay.send_svg(
            svgid="logo",
            svg=tux_svg,
            css="",
            x=50,
            y=270,
            ttl=10,
            font_file='',
        )
```
Where `svg` is full text of the svg, `css` is dynamic CSS to apply, x/y will be left-top corner of the image, dimensions are taken out of SVG itself. `font_file` - if you don't like default fonts used, you can give here absolute path on disk to the font file. It can be repeated many times, it will be loaded only once.

Method `is_svg_supported()` is added too, and can be used similar to multiline detection:
```
def supports_svg(obj) -> bool:
    return (
        hasattr(obj, "is_svg_supported")
        and callable(getattr(obj, "is_svg_supported"))
        and obj.is_svg_supported()
    )
```


`lunasvg` is used to render, so details about supported tags you can find there: https://github.com/sammycage/lunasvg

## Installation

- Clone the repo into your EDMC plugins' directory
  - NB: you *must* name the directory `edmcoverlay`, not `edmcoverlay_for_linux`. You may clone repo elsewhere and symlink it as `edmcoverlay` to the plugins' directory. This is required because all other plugins use this name to access overlay (some are using `EDMCOverlay` naming too).
- Install the dependencies (mostly X11 development headers; on Ubuntu,
  the `xorg-dev` package may be sufficient), `cmake`.
- Run script `create_binary.sh` it will handle all needed. You can edit that script to download emoji font if you don't have one installed (see comments inside).
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
