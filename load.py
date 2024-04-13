"""Totally definitely EDMCOverlay."""

import logging
import time
import tkinter as tk
from pathlib import Path
from subprocess import Popen
from tkinter import ttk

import myNotebook as nb
import plug
from config import appname, config
from ttkHyperlinkLabel import HyperlinkLabel

import os
import sys


import edmcoverlay

plugin_name = Path(__file__).parent.name
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
logger = logging.getLogger(f"{appname}.{plugin_name}")
logger.debug("Loading plugin...")


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
            logger.info("Found overlay binary at \"%s\".", p)
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
        logger.warning("Overlay is already running, skipping the start.")


def __stop_overlay():
    global __overlay_process
    if __overlay_process:
        logger.info("Stopping overlay.")
        __overlay_process.terminate()
        __overlay_process.communicate()
        __overlay_process = None
    else:
        logger.warning("Overlay was not started. Nothing to stop.")


def plugin_start3(plugin_dir):
    logger.info("Python code starts.")
    __start_overlay()
    return "EDMCOverlay for Linux"


def journal_entry(cmdr, is_beta, system, station, entry, state):
    global __overlay_process
    if entry["event"] in ["LoadGame", "StartUp"] and __overlay_process is None:
        logger.info("edmcoverlay2: load event received, starting overlay")
        __start_overlay()
    elif entry["event"] in ["Shutdown", "ShutDown"]:
        logger.info("edmcoverlay2: shutdown event received, stopping overlay")
        __stop_overlay()


def plugin_stop():
    global __overlay_process
    logger.info("Finishing Python's code.")
    edmcoverlay.RequestBinaryToStop()
    __stop_overlay()


def plugin_prefs(parent: nb.Notebook, cmdr: str, is_beta: bool) -> nb.Frame:
    global __xpos_var, __ypos_var, __width_var, __height_var
    __xpos_var = tk.IntVar(value=config.get_int("edmcoverlay2_xpos") or 0)
    __ypos_var = tk.IntVar(value=config.get_int("edmcoverlay2_ypos") or 0)
    __width_var = tk.IntVar(value=config.get_int("edmcoverlay2_width") or 1920)
    __height_var = tk.IntVar(value=config.get_int("edmcoverlay2_height") or 1080)
    frame = nb.Frame(parent)
    frame.columnconfigure(0, weight=1)
    PAD_X = 10
    PAD_Y = 2

    f0 = nb.Frame(frame)
    HyperlinkLabel(
        f0,
        text="edmcoverlay for linux",
        url="https://github.com/alexzk1/edmcoverlay2",
        background=nb.Label().cget("background"),
        underline=True,
    ).grid(row=0, column=0, sticky=tk.W, padx=(PAD_X, 0))
    nb.Label(f0, text="by Ash Holland, Oleksiy Zakharov").grid(
        row=0, column=1, sticky=tk.W, padx=(0, PAD_X)
    )
    f0.grid(sticky=tk.EW)

    ttk.Separator(frame, orient=tk.HORIZONTAL).grid(
        padx=PAD_X, pady=2 * PAD_Y, sticky=tk.EW
    )

    f1 = nb.Frame(frame)
    nb.Label(f1, text="Overlay configuration:").grid(
        row=0, column=0, columnspan=3, padx=PAD_X, pady=PAD_Y, sticky=tk.W
    )
    nb.Label(f1, text="X position").grid(
        row=1, column=0, padx=PAD_X, pady=(PAD_Y, 0), sticky=tk.E
    )
    nb.Entry(f1, textvariable=__xpos_var).grid(
        row=1, column=1, columnspan=3, padx=(0, PAD_X), pady=PAD_Y, sticky=tk.W
    )
    nb.Label(f1, text="Y position").grid(
        row=2, column=0, padx=PAD_X, pady=(PAD_Y, 0), sticky=tk.E
    )
    nb.Entry(f1, textvariable=__ypos_var).grid(
        row=2, column=1, columnspan=3, padx=(0, PAD_X), pady=PAD_Y, sticky=tk.W
    )
    nb.Label(f1, text="Width").grid(
        row=3, column=0, padx=PAD_X, pady=(PAD_Y, 0), sticky=tk.E
    )
    nb.Entry(f1, textvariable=__width_var).grid(
        row=3, column=1, columnspan=3, padx=(0, PAD_X), pady=PAD_Y, sticky=tk.W
    )
    nb.Label(f1, text="Height").grid(
        row=4, column=0, padx=PAD_X, pady=(PAD_Y, 0), sticky=tk.E
    )
    nb.Entry(f1, textvariable=__height_var).grid(
        row=4, column=1, columnspan=3, padx=(0, PAD_X), pady=PAD_Y, sticky=tk.W
    )
    f1.grid(sticky=tk.EW)

    ttk.Separator(frame, orient=tk.HORIZONTAL).grid(
        padx=PAD_X, pady=2 * PAD_Y, sticky=tk.EW
    )

    f2 = nb.Frame(frame)
    nb.Label(f2, text="Manual overlay controls:").grid(
        row=0, column=0, padx=PAD_X, pady=PAD_Y
    )
    nb.Button(f2, text="Start overlay", command=lambda: __start_overlay()).grid(
        row=0, column=1, padx=PAD_X, pady=PAD_Y
    )
    nb.Button(f2, text="Stop overlay", command=lambda: __stop_overlay()).grid(
        row=0, column=2, padx=PAD_X, pady=PAD_Y
    )
    f2.grid(sticky=tk.EW)

    return frame


def prefs_changed(cmdr: str, is_beta: bool) -> None:
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
    if change and __overlay_process is not None:
        logger.info("Settings changes detected, restarting overlay")
        __stop_overlay()
        __start_overlay()
