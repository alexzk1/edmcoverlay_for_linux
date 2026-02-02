import random
import socket
import time
from _logger import logger


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
            return new_sock
        except socket.error as e:
            logger.error(f"Failed to connect to {self.host}:{self.port} - {e}")
            return None

    def send(self, data: bytes) -> bool:
        """Tries to send data with automatic (re) connection when needed.
        Returns true if data was sent.
        """
        for attempt in range(4):
            if self._sock is None:
                self._sock = self._create_socket()

            if self._sock:
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
        if self._sock:
            try:
                self._sock.shutdown(socket.SHUT_RDWR)
                self._sock.close()
            except Exception:
                pass
        self._sock = None
