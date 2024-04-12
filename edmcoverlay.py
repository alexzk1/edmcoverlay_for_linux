import logging
import secrets
from pathlib import Path

from config import appname

plugin_name = Path(__file__).parent.name
logger = logging.getLogger(f"{appname}.{plugin_name}")

logger.debug("edmcoverlay2: lib loaded")

import errno
import json
import socket
import threading
import time
import random

from functools import wraps

def check_game_running():
    return True

class _Overlay:
    _instance = None
    _lock = threading.Lock()
    _initialised = False

    def __new__(cls, *args, **kwargs):
        with cls._lock:
            if cls._instance is None:
                cls._instance = object.__new__(cls, *args, **kwargs)
            return cls._instance

    def __init__(self, server="127.0.0.1", port=5010):
        logger.info("edmcoverlay2: hiiiiiii")
        with self._lock:
            if self._initialised:
                logger.debug("edmcoverlay2: skipping init")
                return
            logger.debug("edmcoverlay2: init")
            self._initialised = True
            self._host = server
            self._port = port        

    def _stop(self):
        self._send_raw_text(inpstr = "NEED_TO_STOP")

    def _send_raw_text(self, inpstr):
        bstr = bytes(inpstr,'UTF-8')
        for retries in range(1, 7):
            try:                
                with self._lock:
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
        self._send_raw_text(inpstr = json.dumps(msg))

    def send_message(self, msgid, text, color, x, y, ttl=4, size="normal"):                               
        msg = {
            "text": text,
            "color": color,
            "x": x,
            "y": y,
            "ttl": ttl,
            "size": size,
            "id": msgid,
        }
        self._send2bin(msg = msg)
            

    def send_shape(self, shapeid, shape, color, fill, x, y, w, h, ttl):           
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
        self._send2bin(msg = msg)
          
class Overlay:
    def __init__(self) -> None:
        self._token = secrets.token_hex(4)
        self._overlay = _Overlay()
    
    def send_raw(self, msg):
        if "msgid" in msg:
            msg["msgid"] = self._token + str(msg["msgid"])
        if "shapeid" in msg:
            msg["shapeid"] = self._token + str(msg["shapeid"])
        if "id" in msg:
            msg["id"] = self._token + msg["id"]
        return self._overlay._send2bin(msg)
           
    def send_message(self, msgid, text, color, x, y, ttl=4, size="normal"):
        return self._overlay.send_message(self._token + msgid, text, color, x, y, ttl=ttl, size=size)

    def send_shape(self, shapeid, shape, color, fill, x, y, w, h, ttl):
        return self._overlay.send_shape(self._token + shapeid, shape, color, fill, x, y, w, h, ttl)

    def connect(self):
        return True

logger.debug("edmcoverlay2: instantiating overlay class")
_the_overlay = _Overlay()

def RequestBinaryToStop():
    _the_overlay._stop()

logger.debug("edmcoverlay2: overlay class instantiated")
