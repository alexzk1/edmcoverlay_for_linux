import random
import socket
import time
from _logger import logger
from overlay_manager import OverlayBinaryManager


class PersistentSocket:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self._sock = None

    def _create_socket(self):
        """Creates and connects socket"""
        try:
            new_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # Send small packages fast
            new_sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            new_sock.settimeout(5.0)
            new_sock.connect((self.host, self.port))
            logger.debug("TCP connection to binary established.")
            return new_sock
        except socket.error:
            return None

    def connect(self) -> bool:
        OverlayBinaryManager().ensure_started(None)
        for attempt in range(3):
            if self._sock is None:
                self._sock = self._create_socket()
                if self._sock is None:
                    logger.debug(f"Server not ready, waiting... (attempt {attempt})")
                    time.sleep(0.2)
                    continue
            else:
                break
        return self._sock is not None

    def send(self, data: bytes) -> bool:
        """Tries to send data with automatic (re) connection when needed.
        Returns true if data was sent.
        """
        # raise Exception("TEST")
        for attempt in range(4):
            if self.connect() and self._sock:
                try:
                    self._sock.sendall(data)
                    return True
                except (socket.error, BrokenPipeError) as e:
                    logger.warning(
                        f"Connection lost ({e}), attempt {attempt} failed, retrying..."
                    )
                    self.close()
                    time.sleep(random.randint(200, 450) / 1000.0)
        return False

    def close(self):
        """Safe socket closing/disconnect."""
        sock = self._sock
        self._sock = None
        if sock:
            logger.debug("Closing socket to binary.")
            try:
                try:
                    sock.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                sock.close()
            except Exception as e:
                logger.debug(f"Socket close error: {e}")
