"""Totally definitely EDMCOverlay."""

import time
import tkinter as tk
from pathlib import Path
from subprocess import Popen
from typing import Optional

import myNotebook as nb
from ttkHyperlinkLabel import HyperlinkLabel

import _config_vars as cfv
import _gui_builder as gb
import edmcoverlay
from _logger import logger
from typing import Tuple

logger.debug("Loading plugin...")

__CaptionText: str = "EDMCOverlay for Linux"
__overlay_process: Optional[Popen] = None
__configVars: cfv.ConfigVars = cfv.ConfigVars()

__configVars.raiseIfWrongNamed()

# This is variable controlled from the main screen. It should not be saved. But it sends message to the running app to hide overlay temporary.
__iHideButDoNotStopOverlay: tk.BooleanVar = tk.BooleanVar(value=False)


logger.debug("Instantiating class OverlayImpl ...")
__the_overlay = edmcoverlay.OverlayImpl()
__the_overlay.setConfig(__configVars)
logger.debug(" class OverlayImpl is instantiated.")


def __find_overlay_binary() -> Path:
    global __configVars
    our_directory = __configVars.getOurDir()

    possible_paths = [
        our_directory / "cpp" / "build" / "edmc_linux_overlay",
        our_directory / "cpp" / "edmc_linux_overlay",
        our_directory / "edmc_linux_overlay",
        our_directory / "overlay",
    ]

    for p in possible_paths:
        if p.exists():
            logger.info('Found overlay binary at "%s".', p)
            return p

    raise RuntimeError("Unable to find overlay's binary.")


def __start_overlay():
    global __overlay_process
    global __configVars
    global __iHideButDoNotStopOverlay

    if not __overlay_process:
        logger.info("Starting overlay.")
        cmd = [
            __find_overlay_binary(),
            str(__configVars.iXPos.get()),
            str(__configVars.iYPos.get()),
            str(__configVars.iWidth.get()),
            str(__configVars.iHeight.get()),
        ]
        if __configVars.iTrackGame.get():
            cmd.append("EliteDangerous64.exe")

        __overlay_process = Popen(cmd)
        time.sleep(2)
        tmp = edmcoverlay.Overlay()
        tmp.send_message(
            "edmcintro",
            "EDMC Overlay for Linux is Ready\n\tNow with multiline support.\n\tNow with SVG images support <-->.\n ",
            "yellow",
            30,
            165,
            ttl=10,
        )

        tux_svg = r"""
    <svg xmlns="http://www.w3.org/2000/svg" width="128" viewBox="0 0 128 180">
        <defs>
            <radialGradient id="space" cx="64" cy="90" r="90" gradientUnits="userSpaceOnUse">
                <stop offset="0" stop-color="#000014" stop-opacity="0.6" />
                <stop offset="1" stop-color="#000000" stop-opacity="0" />
            </radialGradient>
        </defs>
        <rect width="128" height="180" fill="url(#space)" />

        <!-- Stars -->
        <g fill="white">
            <circle cx="20" cy="30" r="1"/>
            <circle cx="110" cy="20" r="1.2"/>
            <circle cx="90" cy="50" r="0.8"/>
            <circle cx="30" cy="80" r="0.6"/>
            <circle cx="70" cy="100" r="0.5"/>
            <circle cx="50" cy="140" r="0.9"/>
        </g>

        <!-- HUD circle (Elite-style) -->
        <circle cx="64" cy="140" r="30" stroke="#00ff99" stroke-width="0.8" fill="none" opacity="0.5" />
        <circle cx="64" cy="140" r="25" stroke="#00ff99" stroke-width="0.5" fill="none" opacity="0.3" />
        <line x1="34" y1="140" x2="94" y2="140" stroke="#00ff99" stroke-width="0.5" opacity="0.2"/>
        <line x1="64" y1="110" x2="64" y2="170" stroke="#00ff99" stroke-width="0.5" opacity="0.2"/>

        <!-- Tux body -->
        <ellipse cx="64" cy="90" rx="20" ry="28" fill="black"/>
        <ellipse cx="64" cy="95" rx="14" ry="22" fill="white"/>

        <!-- Eyes -->
        <circle cx="58" cy="82" r="3.2" fill="white"/>
        <circle cx="70" cy="82" r="3.2" fill="white"/>
        <circle cx="58" cy="82" r="1.2" fill="black"/>
        <circle cx="70" cy="82" r="1.2" fill="black"/>

        <!-- Beak -->
        <polygon points="61,88 67,88 64,92" fill="orange"/>

        <!-- Helmet visor -->
        <path d="M48,75 q16,-25 32,0" fill="none" stroke="#00ccff" stroke-width="1.5" opacity="0.4"/>

        <!-- Flippers (hands) -->
        <path d="M45,100 q-6,10 -2,20" fill="black"/>
        <path d="M83,100 q6,10 2,20" fill="black"/>

        <!-- Controller in flippers -->
        <rect x="54" y="105" width="20" height="6" rx="2" ry="2" fill="#444"/>
        <circle cx="58" cy="108" r="1.2" fill="red"/>
        <circle cx="64" cy="108" r="1.2" fill="green"/>
        <circle cx="70" cy="108" r="1.2" fill="yellow"/>

        <!-- Feet -->
        <path d="M54,116 q-3,10 4,8 t8,0 q4,2 4,-8" fill="orange"/>

        <!-- Label -->
        <text x="64" y="175" fill="limegreen" font-size="12" text-anchor="middle">It works</text>
    </svg>
        """

        tmp.send_svg(
            svgid="logo",
            svg=tux_svg,
            x=50,
            y=270,
            ttl=10,
        )

        __iHideButDoNotStopOverlay.set(False)
    else:
        logger.debug("Overlay is already running, skipping the start.")


def __stop_overlay():
    global __overlay_process
    if __overlay_process:
        logger.info("Stopping overlay binary.")
        __the_overlay._stop()
        time.sleep(1)
        if __overlay_process.poll() is None:
            __overlay_process.terminate()
            __overlay_process.communicate()
        __overlay_process = None
    else:
        logger.debug("Overlay was not started. Nothing to stop.")


# Reaction to the game start/stop.
def journal_entry(cmdr, is_beta, system, station, entry, state):
    global __overlay_process
    if entry["event"] in ["LoadGame", "StartUp"] and __overlay_process is None:
        logger.info("Load event received, ensuring binary overlay is started.")
        __start_overlay()
    elif entry["event"] in ["Shutdown", "ShutDown"]:
        logger.info("Shutdown event received, stopping binary overlay.")
        __stop_overlay()


# Reaction to EDMC start.
def plugin_start3(plugin_dir):
    global __configVars
    logger.info("Python code starts.")
    __configVars.loadFromSettings()

    if __configVars.iDebug.get():
        __start_overlay()

    return __CaptionText


# Reaction to EDMC stop.
def plugin_stop():
    global __overlay_process
    logger.info("Finishing Python's code.")
    __stop_overlay()


def plugin_prefs(parent: nb.Notebook, cmdr: str, is_beta: bool) -> nb.Frame:
    # Settings frame
    global __configVars

    mainFrame = nb.Frame(parent)
    mainFrame.columnconfigure(0, weight=1)

    linkFrame = nb.Frame(mainFrame)
    declareLink = [
        gb.TTextAndInputRow(
            HyperlinkLabel(
                linkFrame,
                text=__CaptionText,
                url="https://github.com/alexzk1/edmcoverlay2",
                background=nb.Label().cget("background"),
                underline=True,
            ),
            None,
        ),
        gb.TTextAndInputRow("by Ash Holland, Oleksiy Zakharov", None),
    ]
    gb.MakeGuiTable(parent=linkFrame, defines=declareLink, initialRaw=0)
    linkFrame.grid(sticky=tk.EW)

    gb.AddMainSeparator(mainFrame)

    inputsFrame = nb.Frame(mainFrame)
    gb.MakeGuiTable(
        parent=inputsFrame, defines=__configVars.getVisualInputs(), initialRaw=0
    )
    inputsFrame.grid(sticky=tk.EW)

    gb.AddMainSeparator(mainFrame)

    startStopFrame = nb.Frame(mainFrame)
    declareButtons = [
        gb.TTextAndInputRow("Manual overlay controls:", None),
        gb.TTextAndInputRow(
            nb.Button(
                startStopFrame,
                text="Start overlay",
                command=lambda: __start_overlay(),
            ),
            nb.Button(
                startStopFrame,
                text="Stop  overlay",
                command=lambda: __stop_overlay(),
            ),
        ),
    ]
    gb.MakeGuiTable(parent=startStopFrame, defines=declareButtons, initialRaw=0)
    startStopFrame.grid(sticky=tk.EW)

    gb.AddMainSeparator(mainFrame)

    return mainFrame


def prefs_changed(cmdr: str, is_beta: bool) -> None:
    global __configVars
    if __configVars.saveToSettings() and __overlay_process is not None:
        __stop_overlay()
        __start_overlay()


def __hide_overlay():
    global __the_overlay
    __the_overlay.send_command("overlay_off")


def __show_overlay():
    global __the_overlay
    __the_overlay.send_command("overlay_on")


def plugin_app(parent: tk.Frame) -> Tuple[tk.Radiobutton, tk.Radiobutton]:
    # Main window frame

    global __iHideButDoNotStopOverlay

    btnShow = tk.Radiobutton(
        parent,
        variable=__iHideButDoNotStopOverlay,
        value=False,
        text="Show Overlay",
        command=lambda: __show_overlay(),
    )

    btnHide = tk.Radiobutton(
        parent,
        variable=__iHideButDoNotStopOverlay,
        value=True,
        text="Hide Overlay",
        command=lambda: __hide_overlay(),
    )
    return btnShow, btnHide
