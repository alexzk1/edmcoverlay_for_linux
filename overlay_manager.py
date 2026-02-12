from collections.abc import Callable
import threading
from subprocess import Popen
from typing import List, Optional
from _logger import logger
import socket
import time


class OverlayBinaryManager:
    """Singltone to start overlay binary. On 1st construction host, port
    (for connection check purposes) must be provided and function-generator
    of the command line for the binary."""

    _instance: Optional["OverlayBinaryManager"] = None
    _lock = threading.Lock()  # Lock to access instance
    _process_lock = threading.Lock()  # Lock to access process.

    def __new__(cls, *args, **kwargs):
        with cls._lock:
            if cls._instance is None:
                cls._instance = super(OverlayBinaryManager, cls).__new__(cls)
                cls._instance._configured = False
        return cls._instance

    def __init__(
        self,
        host: Optional[str] = None,  # Used to check connection
        port: Optional[int] = None,  # Used to check connection
        cmd_factory: Optional[
            Callable[[], List[str]]
        ] = None,  # Defines CMD for binary.
    ):
        if self._configured:
            return
        if cmd_factory is None or host is None or port is None:
            logger.error(
                "OverlayBinaryManager must be called with all parameters set for 1st time."
            )
            return
        with self._lock:
            if not self._configured:
                self.cmd_factory = cmd_factory
                self.host = host
                self.port = port
                self._process = None
                self._configured = True
                logger.debug(f"OverlayManager configured for {host}:{port}")

    def ensure_started(self, post_start_callback: Optional[Callable] = None):
        """
        Ensures process is running.
        cmd_factory: factory function which returns cmd line parameters for the process
        """

        was_alive = False
        server_ready = False

        with self._process_lock:
            if self._process is not None:
                if self._process.poll() is not None:
                    logger.debug("Overlay binary process is dead. Cleaning up.")
                    self._process = None

            was_alive = self._process is not None
            server_ready = was_alive

            if not was_alive:
                logger.info("Starting overlay binary...")
                try:
                    cmd = self.cmd_factory()
                    self._process = Popen(cmd)
                    server_ready = self._wait_for_port()
                except Exception as e:
                    logger.error(f"Failed to launch binary: {e}")
                    return

        if was_alive != server_ready:
            if server_ready:
                logger.info("Server is up! Running post-start tasks.")
                if post_start_callback is not None:
                    post_start_callback()
            else:
                logger.error("Overlay started but port is closed after 10s.")
                # self.stop()

    def stop(self):
        """Thread safe process stop."""
        with self._process_lock:
            if self._process:
                logger.info("Stopping overlay binary.")
                if self._process.poll() is None:
                    self._process.terminate()
                    self._process.wait(timeout=5)
                    # Probably, communicate is not needed as we used wait().
                    # self._process.communicate()
                self._process = None
                logger.info("Overlay binary was stopped.")
            else:
                logger.debug("Overlay binary was not started.")

    @property
    def is_alive(self) -> bool:
        with self._process_lock:
            return self._process is not None and self._process.poll() is None

    def _wait_for_port(self) -> bool:
        """Awaiting server is up."""
        if self._process is not None:
            for _ in range(20):
                try:
                    with socket.create_connection((self.host, self.port), timeout=0.5):
                        return True
                except (socket.error, ConnectionRefusedError):
                    if self._process.poll() is not None:
                        return False
                    time.sleep(0.5)
        return False
