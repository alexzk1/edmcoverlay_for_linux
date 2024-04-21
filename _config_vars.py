import collections
import json
import logging
import tkinter as tk
from pathlib import Path

from config import config

import myNotebook as nb
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
    __defaultNormalFont: int = 16
    __defaultLargeFont: int = 20
    __fontSizeTooSmall = 4

    # Possible spellings. Some plugins may try to use Windows' file name.
    # So we can name this plugin as 1 of those, however either way will break other half of users.
    __required_plugin_dir: list[str] = ["edmcoverlay", "EDMCOverlay"]
    __json_config_name: str = "edmc_linux_overlay_json"
    __binaryReloadRequired: bool = False

    # Simple config fields
    iXPos: tk.IntVar = tk.IntVar(value=0)
    iYPos: tk.IntVar = tk.IntVar(value=0)
    iWidth: tk.IntVar = tk.IntVar(value=1920)
    iHeight: tk.IntVar = tk.IntVar(value=1080)
    _iFontNorm: tk.IntVar = tk.IntVar(value=__defaultNormalFont)
    _iFontLarge: tk.IntVar = tk.IntVar(value=__defaultLargeFont)
    _iFontPerPlugin: dict = {}

    iDebug: tk.BooleanVar = tk.BooleanVar(value=False)

    def __init__(self) -> None:
        # Find installed plugins. We will keep font per plugin.
        pluginsDir = self.getPluginsDir()
        ourDir = self.getOurDir()

        logger.debug('Plugins dir: "%s"', pluginsDir)

        for child in pluginsDir.iterdir():
            if isDirOrSymlinkToDir(child) and not ourDir.samefile(child):
                logger.debug('Found EDMC plugin: "%s"', child.name)
                self._iFontPerPlugin[str(child.name)] = tk.IntVar(value=0)

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
            self.__TJsonFieldMapper("fontN", self._iFontNorm, False),
            self.__TJsonFieldMapper("fontL", self._iFontLarge, False),
            self.__TJsonFieldMapper("fontNPerPlagun", self._iFontPerPlugin, False),
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
        logger.info("Set loglevel to: %i", logger.level)

    def loadFromSettings(self):
        """Loads stored settings."""

        loaded_str = config.get_str(self.__json_config_name)
        if loaded_str is not None:
            obj = json.loads(loaded_str)
            for m in self.__getJson2FieldMapper():
                if m.json_name in obj:
                    if isinstance(obj[m.json_name], dict):
                        for k in obj[m.json_name]:
                            m.field_ref[k].set(obj[m.json_name][k])
                    else:
                        m.field_ref.set(obj[m.json_name])

        self.__debug_switched("", 0, "")
        self.__binaryReloadRequired = False

    def saveToSettings(self) -> bool:
        """Saves variables to settings. Returns True if binary must be reloaded."""

        # Building python's dictionary which will be dumped to the single json.
        output = {}
        for var in self.__getJson2FieldMapper():
            if isinstance(var.field_ref, dict):
                d = {}
                for k in var.field_ref:
                    d[k] = var.field_ref[k].get()
                output[var.json_name] = d
            else:
                output[var.json_name] = var.field_ref.get()

        config.set(self.__json_config_name, json.dumps(output, separators=(",", ":")))

        requiredReload = self.__binaryReloadRequired
        self.__binaryReloadRequired = False
        return requiredReload

    def getVisualInputs(self):
        arr = [
            gb.TTextAndInputRow("Overlay Configuration:", None),
            gb.TTextAndInputRow("X Position", self.iXPos),
            gb.TTextAndInputRow("Y Position", self.iYPos),
            gb.TTextAndInputRow("Width", self.iWidth),
            gb.TTextAndInputRow("Height", self.iHeight),
            gb.TTextAndInputRow("Default Fonts' Sizes:", None),
            gb.TTextAndInputRow("Font Normal", self._iFontNorm),
            gb.TTextAndInputRow("Font Large", self._iFontLarge),
            gb.TTextAndInputRow("N. Font Per Plugin (L. = +4, default = 0):", None),
        ]
        for key in self._iFontPerPlugin:
            arr.append(gb.TTextAndInputRow(key, self._iFontPerPlugin[key]))

        arr.append(gb.TTextAndInputRow("", None))
        arr.append(gb.TTextAndInputRow("Debug", self.iDebug))
        return arr

    def getOurDir(self):
        tk.OptionMenu
        return Path(__file__).parent.resolve()

    def getPluginsDir(self):
        return Path(__file__).parent.parent.resolve()

    def raiseIfWrongNamed(self):
        found = False
        pluginsDir = self.getPluginsDir()
        ourDir = self.getOurDir()
        for possibleName in self.__required_plugin_dir:
            expectedName = pluginsDir / possibleName
            found = found or (expectedName.exists() and ourDir.samefile(expectedName))

        if not found:
            raise WrongFoldeNameException(
                "Please rename overlay's folder to " + self.__required_plugin_dir[0]
            )

    def __isRequestedLarge(self, requested: str) -> bool:
        return requested == "large"

    def __minimalFont(self, font: int, default: int) -> int:
        if font < self.__fontSizeTooSmall:
            font = self._iFontNorm.get()
        if font < self.__fontSizeTooSmall:
            font = default
        return font

    def getFontSize(self, ownerPath: str, requested: str) -> int:
        logger.debug('Requested font size for "%s"', ownerPath)
        for key in self._iFontPerPlugin:
            if key in ownerPath:
                normal = self._iFontPerPlugin[key].get()
                if normal < self.__fontSizeTooSmall:
                    break
                if self.__isRequestedLarge(requested):
                    return self.__minimalFont(normal + 4, self.__defaultLargeFont)
                return self.__minimalFont(normal, self.__defaultNormalFont)

        if self.__isRequestedLarge(requested):
            return self.__minimalFont(self._iFontLarge.get(), self.__defaultLargeFont)
        return self.__minimalFont(self._iFontNorm.get(), self.__defaultNormalFont)
