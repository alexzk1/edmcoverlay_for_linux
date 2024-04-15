"""Totally definitely EDMCOverlay."""

import time
import tkinter as tk
from pathlib import Path
from subprocess import Popen

import myNotebook as nb
from ttkHyperlinkLabel import HyperlinkLabel

import _config_vars as cfv
import _gui_builder as gb
import edmcoverlay
from _logger import logger

logger.debug("Loading plugin...")

__CaptionText: str = "EDMCOverlay for Linux"
__overlay_process: Popen = None
__configVars: cfv.ConfigVars = cfv.ConfigVars()

__configVars.raiseIfWrongNamed()

def __find_overlay_binary() -> Path:
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
    if not __overlay_process:
        logger.info("Starting overlay.")
        __overlay_process = Popen(
            [
                __find_overlay_binary(),
                str(__configVars.iXPos.get()),
                str(__configVars.iYPos.get()),
                str(__configVars.iWidth.get()),
                str(__configVars.iHeight.get()),
            ]
        )
        time.sleep(2)
        tmp = edmcoverlay.Overlay()
        tmp.send_message(
            "edmcintro", "EDMC Overlay for Linux is Ready", "yellow", 30, 165, ttl=10
        )
    else:
        logger.debug("Overlay is already running, skipping the start.")


def __stop_overlay():
    global __overlay_process
    if __overlay_process:
        logger.info("Stopping overlay.")
        edmcoverlay.RequestBinaryToStop()
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
    __configVars.saveToSettings()
    if __overlay_process is not None:
        __stop_overlay()
        __start_overlay()
