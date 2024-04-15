"""Totally definitely EDMCOverlay."""

import time
import tkinter as tk
from pathlib import Path
from subprocess import Popen
from tkinter import ttk

import myNotebook as nb
from config import config
from ttkHyperlinkLabel import HyperlinkLabel

import _gui_builder as gb
import edmcoverlay
from _logger import logger

logger.debug("Loading plugin...")

__CaptionText: str = "EDMCOverlay for Linux"
__overlay_process: Popen = None
__xpos_var: tk.IntVar
__ypos_var: tk.IntVar
__width_var: tk.IntVar
__height_var: tk.IntVar


def __find_overlay_binary() -> Path:
    our_directory = Path(__file__).resolve().parent

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
        logger.info("Starting overlay")
        xpos = config.get_int("edmcoverlay2_xpos") or 0
        ypos = config.get_int("edmcoverlay2_ypos") or 0
        width = config.get_int("edmcoverlay2_width") or 1920
        height = config.get_int("edmcoverlay2_height") or 1080
        __overlay_process = Popen(
            [__find_overlay_binary(), str(xpos), str(ypos), str(width), str(height)]
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
    __start_overlay()
    return __CaptionText


# Reaction to EDMC stop.
def plugin_stop():
    global __overlay_process
    logger.info("Finishing Python's code.")
    __stop_overlay()


def __update_config() -> bool:
    xpos = __xpos_var.get()
    ypos = __ypos_var.get()
    width = __width_var.get()
    height = __height_var.get()
    change = False
    for name, val in [
        ("xpos", xpos),
        ("ypos", ypos),
        ("width", width),
        ("height", height),
    ]:
        try:
            assert int(val) >= 0
        except (ValueError, AssertionError):
            logger.warning("Bad config value for %s: %r", name, val)
        else:
            try:
                old_val = config.get_int(f"edmcoverlay2_{name}")
            except (TypeError, ValueError):
                pass
            else:
                if val != old_val:
                    change = True
            config.set(f"edmcoverlay2_{name}", val)
    return change


def __update_and_start():
    __update_config()
    __start_overlay()


def __update_and_stop():
    __update_config()
    __stop_overlay()


def plugin_prefs(parent: nb.Notebook, cmdr: str, is_beta: bool) -> nb.Frame:
    global __xpos_var, __ypos_var, __width_var, __height_var
    __xpos_var = tk.IntVar(value=config.get_int("edmcoverlay2_xpos") or 0)
    __ypos_var = tk.IntVar(value=config.get_int("edmcoverlay2_ypos") or 0)
    __width_var = tk.IntVar(value=config.get_int("edmcoverlay2_width") or 1920)
    __height_var = tk.IntVar(value=config.get_int("edmcoverlay2_height") or 1080)

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
    declareInputs = [
        gb.TTextAndInputRow("Overlay configuration:", None),
        gb.TTextAndInputRow("X position", __xpos_var),
        gb.TTextAndInputRow("Y position", __ypos_var),
        gb.TTextAndInputRow("Width", __width_var),
        gb.TTextAndInputRow("Height", __height_var),
    ]
    gb.MakeGuiTable(parent=inputsFrame, defines=declareInputs, initialRaw=0)
    inputsFrame.grid(sticky=tk.EW)

    gb.AddMainSeparator(mainFrame)

    startStopFrame = nb.Frame(mainFrame)
    declareButtons = [
        gb.TTextAndInputRow("Manual overlay controls:", None),
        gb.TTextAndInputRow(
            nb.Button(
                startStopFrame,
                text="Start overlay",
                command=lambda: __update_and_start(),
            ),
            nb.Button(
                startStopFrame,
                text="Stop  overlay",
                command=lambda: __update_and_stop(),
            ),
        ),
    ]
    gb.MakeGuiTable(parent=startStopFrame, defines=declareButtons, initialRaw=0)
    startStopFrame.grid(sticky=tk.EW)

    gb.AddMainSeparator(mainFrame)

    return mainFrame


def prefs_changed(cmdr: str, is_beta: bool) -> None:
    if __update_config() and __overlay_process is not None:
        logger.info("Settings changes detected, restarting overlay")
        __stop_overlay()
        __start_overlay()
