from _logger import logger

import secrets
import inspect
import errno
import json
import socket
import threading
import time
import random
from monitor import monitor
import _config_vars


def check_game_running():
    return monitor.game_running()


class OverlayImpl:
    __instance = None
    __lock = threading.Lock()
    __initialised: bool = False
    __config: _config_vars.ConfigVars = None

    def __new__(cls, *args, **kwargs):
        with cls.__lock:
            if cls.__instance is None:
                cls.__instance = object.__new__(cls, *args, **kwargs)
            return cls.__instance

    def __init__(
        self,
        server: str = "127.0.0.1",
        port: int = 5010,
    ):
        logger.info("Loading implementation details...")
        with self.__lock:
            if self.__initialised:
                logger.debug("Details were already loaded.")
                return
            logger.debug("Initializing implementation details.")
            self.__initialised = True
            self._host = server
            self._port = port

    def setConfig(self, config: _config_vars.ConfigVars):
        self.__config = config

    def _stop(self):
        logger.info("Sending self-stop/exit request to the binary.")
        self.send_command("exit")

    def _send_raw_text(self, inpstr: str):
        # print("Raw inpstr: ", inpstr)
        inpstr = (
            inpstr.replace("\\u202f", "\\u00a0")
            .replace("\\ud83d\\udcc8", "*")
            .replace("\\ud83d\\udcdd", "»")
        )

        bstr = inpstr.encode("utf-8")
        for retries in range(1, 7):
            try:
                with self.__lock:
                    conn = socket.socket()
                    conn.connect((self._host, self._port))
                    conn.send(str(len(bstr)).encode("utf-8") + b"#" + bstr)
                    conn.close()
                    break
            except socket.error as e:
                if e.errno == errno.ECONNREFUSED:
                    logger.warning(
                        "Connection to binary server was refused %i time(s).", retries
                    )
                    time.sleep(random.randint(200, 450) / 1000.0)
                else:
                    raise
        return None

    def _send2bin(self, owner: str, msg: dict):
        if "font_size" not in msg and "shape" not in msg:
            font = msg.get("size", "normal")
            msg["font_size"] = self.__config.getFontSize(owner, font)
        self._send_raw_text(json.dumps(msg))

    def send_command(self, command: str):
        self._send_raw_text(json.dumps({"command": command}))

    def send_message(
        self,
        owner: str,
        msgid: str,
        text: str,
        color: str,
        x: int,
        y: int,
        ttl: int = 4,
        size: str = "normal",
    ):
        msg = {
            "text": text,
            "color": color,
            "x": x,
            "y": y,
            "ttl": ttl,
            "size": size,
            "id": msgid,
        }
        self._send2bin(owner, msg)

    def send_shape(
        self,
        owner: str,
        shapeid: str,
        shape: str,
        color: str,
        fill: str,
        x: int,
        y: int,
        w: int,
        h: int,
        ttl: int,
    ):
        msg = {
            "shape": shape,
            "color": color,
            "fill": fill,
            "x": x,
            "y": y,
            "w": w,
            "h": h,
            "ttl": ttl,
            "id": shapeid,
        }
        self._send2bin(owner, msg)


class Overlay:
    __caller_path: str = ""
    __token: str = ""
    __overlay: OverlayImpl = None

    def __init__(self) -> None:
        self.__token = secrets.token_hex(4)
        self.__overlay = OverlayImpl()
        callFrames = inspect.getouterframes(inspect.currentframe())
        self.__caller_path = callFrames[1].filename
        logger.debug('\tOverlay() is created for plugin: "%s"\n', self.__caller_path)

    def send_raw(self, msg: dict):
        if "msgid" in msg:
            msg["msgid"] = self.__token + str(msg["msgid"])
        if "shapeid" in msg:
            msg["shapeid"] = self.__token + str(msg["shapeid"])
        if "id" in msg:
            msg["id"] = self.__token + msg["id"]
        return self.__overlay._send2bin(self.__caller_path, msg)

    def send_command(self, command: str):
        self.__overlay.send_command(command)

    def send_message(
        self,
        msgid: str,
        text: str,
        color: str,
        x: int,
        y: int,
        ttl: int = 4,
        size="normal",
    ):
        return self.__overlay.send_message(
            self.__caller_path,
            self.__token + msgid,
            text,
            color,
            x,
            y,
            ttl=ttl,
            size=size,
        )

    def send_shape(
        self,
        shapeid: str,
        shape: str,
        color: str,
        fill: str,
        x: int,
        y: int,
        w: int,
        h: int,
        ttl: int,
    ):
        return self.__overlay.send_shape(
            owner=self.__caller_path,
            shapeid=self.__token + shapeid,
            shape=shape,
            color=color,
            fill=fill,
            x=x,
            y=y,
            w=w,
            h=h,
            ttl=ttl,
        )

    def connect(self) -> bool:
        return True
