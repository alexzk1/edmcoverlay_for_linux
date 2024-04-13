from pathlib import Path
from config import appname
from functools import wraps

import logging
import secrets
import inspect
import errno
import json
import socket
import threading
import time
import random



plugin_name = Path(__file__).parent.name
logger = logging.getLogger(f"{appname}.{plugin_name}")
logger.debug("edmcoverlay2: lib loaded")
def check_game_running():
    return True

class _OverlayImpl:
    __instance = None
    __lock = threading.Lock()
    __initialised: bool = False

    def __new__(cls, *args, **kwargs):
        with cls.__lock:
            if cls.__instance is None:
                cls.__instance = object.__new__(cls, *args, **kwargs)
            return cls.__instance

    def __init__(self, server: str = "127.0.0.1", port: int = 5010):
        logger.info("edmcoverlay2: loaded python plugin.")
        with self.__lock:
            if self.__initialised:
                logger.debug("edmcoverlay2: skipping init")
                return
            logger.debug("edmcoverlay2: init")
            self.__initialised = True
            self._host = server
            self._port = port

    def _stop(self):
        self._send_raw_text("NEED_TO_STOP")

    def _send_raw_text(self, inpstr: str):
        bstr = bytes(inpstr, "UTF-8")
        for retries in range(1, 7):
            try:
                with self.__lock:
                    conn = socket.socket()
                    conn.connect((self._host, self._port))
                    conn.send(str(len(bstr)).encode() + b"#" + bstr)
                    conn.close()
                    break
            except socket.error as e:
                if e.errno == errno.ECONNREFUSED:
                    logger.warning("edmcoverlay2: conn refused times %i", retries)
                    time.sleep(random.randint(200, 450) / 1000.0)
                else:
                    raise
        return None

    def _send2bin(self, msg):
        self._send_raw_text(json.dumps(msg))

    def send_message(
        self,
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
        self._send2bin(msg)

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
        self._send2bin(msg)


class Overlay:
    __caller_path: str = None
    __token: str = None
    __overlay: _OverlayImpl = None

    def __init__(self) -> None:
        self.__token = secrets.token_hex(4)
        self.__overlay = _OverlayImpl()
        callFrames = inspect.getouterframes(inspect.currentframe())
        __caller_path = callFrames[1].filename
        logger.debug('\tOverlay() is created from: "%s"\n', __caller_path)

    def send_raw(self, msg):
        if "msgid" in msg:
            msg["msgid"] = self.__token + str(msg["msgid"])
        if "shapeid" in msg:
            msg["shapeid"] = self.__token + str(msg["shapeid"])
        if "id" in msg:
            msg["id"] = self.__token + msg["id"]
        return self.__overlay._send2bin(msg)

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
            self.__token + msgid, text, color, x, y, ttl=ttl, size=size
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
            self.__token + shapeid, shape, color, fill, x, y, w, h, ttl
        )

    def connect(self) -> bool:
        return True


logger.debug("edmcoverlay2: instantiating overlay class")
__the_overlay = _OverlayImpl()


def RequestBinaryToStop():
    __the_overlay._stop()


logger.debug("edmcoverlay2: overlay class instantiated")
