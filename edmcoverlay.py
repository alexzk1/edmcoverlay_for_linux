import queue
import threading
from _logger import logger

import secrets
import inspect
import json
import _config_vars
from typing import Optional

from persistent_socket import PersistentSocket

SERVER_IP = "127.0.0.1"
SERVER_PORT = 5010

_SHUTDOWN_SIGNAL = object()


class OverlayImpl:
    """Implementation of communication with binary overlay server.
    Note, it is NOT singltone so each plugin will have own copy.
    If plugin breaks protocol, it will be disconnected just
    while others remain connected.
    """

    _config: Optional[_config_vars.ConfigVars] = None

    def __init__(
        self,
        server: str = SERVER_IP,
        port: int = SERVER_PORT,
    ):
        self._host = server
        self._port = port
        self._connection = PersistentSocket(server, port)

        self._queue = queue.Queue()
        self._worker_thread = threading.Thread(
            target=self._send_worker,
            name=f"OverlayWorker-{secrets.token_hex(2)}",
            daemon=True,
        )
        self._worker_thread.start()

    @staticmethod
    def setConfig(config: _config_vars.ConfigVars):
        OverlayImpl._config = config

    def _stop(self):
        logger.info("Sending self-stop/exit request to the binary.")
        self.send_command("exit")
        self.closeConnection()

    def _send_worker(self):
        """Background thread to send data to binary overlay."""
        while True:
            data = self._queue.get()
            if data is _SHUTDOWN_SIGNAL:
                self._connection.close()
                break
            try:
                bstr = data.encode("utf-8")
                ok = self._connection.send(str(len(bstr)).encode("utf-8") + b"#" + bstr)
                if not ok:
                    logger.warning("Overlay Worker: Could not send data to binary.")
            except Exception as e:
                logger.error(f"Overlay Worker thread error: {e}")
            finally:
                self._queue.task_done()

    def _send_raw_text(self, inpstr: str):
        self._queue.put(inpstr)
        return None

    def _send2bin(self, owner: str, msg: dict):
        if self._config is not None:
            if "font_size" not in msg and "shape" not in msg and "svg" not in msg:
                font = msg.get("size", "normal")
                msg["font_size"] = self._config.getFontSize(owner, font)

            if "vector_font_size" not in msg and "vector" in msg:
                font = msg.get("size", "normal")
                msg["vector_font_size"] = self._config.getFontSize(owner, font)

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

    def send_svg(
        self,
        owner: str,
        svgid: str,
        svg: str,
        css: str,
        font_file: str,
        x: int,
        y: int,
        ttl: int,
    ):
        msg = {
            "svg": svg,
            "css": css,
            "x": x,
            "y": y,
            "ttl": ttl,
            "id": svgid,
            "font_file": font_file,
        }
        self._send2bin(owner, msg)

    def closeConnection(self):
        self._queue.put(_SHUTDOWN_SIGNAL)

    def connect(self):
        return self._connection.connect()


class Overlay:
    """Main class to communicated with binary overlay.
    Plugin must create a copy of this class to draw overlay messages.
    Each new copy will establish new TCP connection.
    """

    __caller_path: str = ""
    __token: str = ""

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
        if "svgid" in msg:
            msg["svgid"] = self.__token + str(msg["svgid"])
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

    def send_svg(
        self,
        svgid: str,
        svg: str,
        x: int,
        y: int,
        ttl: int,
        css: str = "",
        font_file: str = "",
    ):
        return self.__overlay.send_svg(
            owner=self.__caller_path,
            svgid=self.__token + svgid,
            svg=svg,
            css=css,
            font_file=font_file,
            x=x,
            y=y,
            ttl=ttl,
        )

    def connect(self) -> bool:
        return self.__overlay.connect()

    def is_multiline_supported(self) -> bool:
        """
        Returns:
            bool: true if this plugin can draw multilined text direct, separated with \n.
        """
        return True

    def is_svg_supported(self) -> bool:
        """
        Returns:
            bool: true if this plugin can render SVG direct.
        """
        return True

    def is_emoji_supported(self) -> bool:
        """
        Returns:
            bool: true if this plugin can render emoji in texts.
        """
        return True
