import collections
import json
import logging
import tkinter as tk
from pathlib import Path

from config import config

import _gui_builder as gb
import _logger as lgr
from _logger import logger


class WrongFoldeNameException(RuntimeError): ...


def isDirOrSymlinkToDir(path: Path):
    return path.is_dir() or (path.is_symlink() and path.resolve().is_dir())


class ConfigVars:
    __TJsonFieldMapper = collections.namedtuple(
        "__TJsonFieldMapper", "json_name field_ref reload_needed"
    )

    __required_plugin_dir: str = "edmcoverlay"
    __json_config_name: str = "edmc_linux_overlay_json"
    __installedPlugins: list[str] = []
    __binaryReloadRequired: bool = False

    iXPos: tk.IntVar = tk.IntVar(value=0)
    iYPos: tk.IntVar = tk.IntVar(value=0)
    iWidth: tk.IntVar = tk.IntVar(value=1920)
    iHeight: tk.IntVar = tk.IntVar(value=1080)

    iFontNorm: tk.IntVar = tk.IntVar(value=16)
    iFontLarge: tk.IntVar = tk.IntVar(value=20)

    iDebug: tk.BooleanVar = tk.BooleanVar(value=False)

    def __init__(self) -> None:
        self.__listInstalledEDMCPlugins()

        # Install callback once because trace_add does not remove existing callback(s().
        for m in self.__getJson2FieldMapper():
            if m.reload_needed:
                m.field_ref.trace_add("write", self.__set_changed)
                m.field_ref.trace_add("unset", self.__set_changed)

        self.iDebug.trace_add("write", self.__debug_switched)

    # This must be in sync with declared fields.
    def __getJson2FieldMapper(self):
        return [
            self.__TJsonFieldMapper("xpos", self.iXPos, True),
            self.__TJsonFieldMapper("ypos", self.iYPos, True),
            self.__TJsonFieldMapper("width", self.iWidth, True),
            self.__TJsonFieldMapper("height", self.iHeight, True),
            self.__TJsonFieldMapper("fontN", self.iFontNorm, False),
            self.__TJsonFieldMapper("fontL", self.iFontLarge, False),
            self.__TJsonFieldMapper("debug", self.iDebug, False),
        ]

    def __set_changed(self, var, index, mode):
        self.__binaryReloadRequired = True

    def __debug_switched(self, var, index, mode):
        # Debug switch changes output in log.
        if self.iDebug.get():
            logger.setLevel(logging.DEBUG)
        else:
            logger.setLevel(lgr.DEFAULT_LOG_LEVEL)

        logger.info("Set debug mode to: %i", logger.level)

    def loadFromSettings(self):
        """Loads stored settings."""

        loaded_str = config.get_str(self.__json_config_name)
        if loaded_str is not None:
            obj = json.loads(loaded_str)
            for m in self.__getJson2FieldMapper():
                if m.json_name in obj:
                    m.field_ref.set(obj[m.json_name])

        self.__debug_switched("", 0, "")
        self.__binaryReloadRequired = False

    def saveToSettings(self) -> bool:
        """Saves variables to settings. Returns True if binary must be reloaded."""

        output = {}
        for var in self.__getJson2FieldMapper():
            output[var.json_name] = var.field_ref.get()
        config.set(self.__json_config_name, json.dumps(output, separators=(",", ":")))

        requiredReload = self.__binaryReloadRequired
        self.__binaryReloadRequired = False
        return requiredReload

    def getVisualInputs(self):
        return [
            gb.TTextAndInputRow("Overlay Configuration:", None),
            gb.TTextAndInputRow("X Position", self.iXPos),
            gb.TTextAndInputRow("Y Position", self.iYPos),
            gb.TTextAndInputRow("Width", self.iWidth),
            gb.TTextAndInputRow("Height", self.iHeight),
            gb.TTextAndInputRow("Default Fonts' Sizes:", None),
            gb.TTextAndInputRow("Font Normal", self.iFontNorm),
            gb.TTextAndInputRow("Font Large", self.iFontLarge),
            gb.TTextAndInputRow("", None),
            gb.TTextAndInputRow("Debug", self.iDebug),
        ]

    def getOurDir(self):
        return Path(__file__).parent.resolve()

    def getPluginsDir(self):
        return Path(__file__).parent.parent.resolve()

    def __listInstalledEDMCPlugins(self):
        pluginsDir = self.getPluginsDir()
        ourDir = self.getOurDir()

        logger.debug('Plugins dir: "%s"', pluginsDir)

        for child in pluginsDir.iterdir():
            if isDirOrSymlinkToDir(child) and not ourDir.samefile(child):
                logger.debug('Found EDMC plugin: "%s"', child.name)
                self.__installedPlugins.append(str(child.name))

    def raiseIfWrongNamed(self):
        expectedName = self.getPluginsDir() / self.__required_plugin_dir
        if not (expectedName.exists() and self.getOurDir().samefile(expectedName)):
            raise WrongFoldeNameException(
                "Please rename overlay's folder to " + self.__required_plugin_dir
            )

    def getFontSize(self, ownerPath: str, requested: str) -> int:
        if requested == "large":
            return self.iFontLarge.get()

        # TODO: finish per-plugin fonts' config.

        return self.iFontNorm.get()
