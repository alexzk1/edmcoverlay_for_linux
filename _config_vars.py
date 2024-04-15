import collections
import json
import tkinter as tk
from pathlib import Path

from config import config

import _gui_builder as gb
from _logger import logger


class WrongFoldeNameException(RuntimeError): ...


def isDirOrSymlinkToDir(path: Path):
    return path.is_dir() or (path.is_symlink() and path.resolve().is_dir())


class ConfigVars:
    __required_plugin_dir = "edmcoverlay"
    __installedPlugins = []
    __json_config_name: str = "edmc_linux_overlay_json"
    __TJsonFieldMapper = collections.namedtuple(
        "__TJsonFieldMapper", "json_name field_ref"
    )

    iXPos: tk.IntVar = tk.IntVar(value=0)
    iYPos: tk.IntVar = tk.IntVar(value=0)
    iWidth: tk.IntVar = tk.IntVar(value=1920)
    iHeight: tk.IntVar = tk.IntVar(value=1080)
    iDebug: tk.BooleanVar = tk.BooleanVar(value=False)

    def __init__(self) -> None:
        self.__installedPlugins = self.listInstalledEDMCPlugins()

    # this must be in sync with declared fields
    def __getJson2FieldMapper(self):
        return [
            self.__TJsonFieldMapper("xpos", self.iXPos),
            self.__TJsonFieldMapper("ypos", self.iYPos),
            self.__TJsonFieldMapper("width", self.iWidth),
            self.__TJsonFieldMapper("height", self.iHeight),
            self.__TJsonFieldMapper("debug", self.iDebug),
        ]

    def loadFromSettings(self):
        """Loads stored settings."""

        loaded_str = config.get_str(self.__json_config_name)
        if loaded_str is not None:
            obj = json.loads(loaded_str)
            for m in self.__getJson2FieldMapper():
                if m.json_name in obj:
                    m.field_ref.set(obj[m.json_name])

    def saveToSettings(self):
        """Saves variables to settings."""

        output = {}
        for var in self.__getJson2FieldMapper():
            output[var.json_name] = var.field_ref.get()
        config.set(self.__json_config_name, json.dumps(output, separators=(",", ":")))

    def getVisualInputs(self):
        return [
            gb.TTextAndInputRow("Overlay configuration:", None),
            gb.TTextAndInputRow("X position", self.iXPos),
            gb.TTextAndInputRow("Y position", self.iYPos),
            gb.TTextAndInputRow("Width", self.iWidth),
            gb.TTextAndInputRow("Height", self.iHeight),
            gb.TTextAndInputRow("Debug", self.iDebug),
        ]

    def getOurDir(self):
        return Path(__file__).parent.resolve()

    def getPluginsDir(self):
        return Path(__file__).parent.parent.resolve()

    def listInstalledEDMCPlugins(self):
        pluginsDir = self.getPluginsDir()
        ourDir = self.getOurDir()

        logger.debug('Plugins dir: "%s"', pluginsDir)
        dirs = []
        for child in pluginsDir.iterdir():
            if isDirOrSymlinkToDir(child) and not ourDir.samefile(child):
                logger.debug('Found EDMC plugin: "%s"', child.name)
                dirs.append(str(child.name))

        return dirs

    def raiseIfWrongNamed(self):
        expectedName = self.getPluginsDir() / self.__required_plugin_dir
        if not (expectedName.exists() and self.getOurDir().samefile(expectedName)):
            raise WrongFoldeNameException(
                "Please rename overlay's folder to " + self.__required_plugin_dir
            )
