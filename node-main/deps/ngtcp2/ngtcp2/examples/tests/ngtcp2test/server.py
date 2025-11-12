import logging
import os
import re
import subprocess
import time
from datetime import datetime, timedelta
from threading import Thread

from .tls import HandShake
from .env import Env, CryptoLib
from .log import LogFile, HexDumpScanner


log = logging.getLogger(__name__)


class ServerRun:

    def __init__(self, env: Env, logfile: LogFile):
        self.env = env
        self._logfile = logfile
        self.log_lines = self._logfile.get_recent()
        self._data_recs = None
        self._hs_recs = None
        if self.env.verbose > 1:
            log.debug(f'read {len(self.log_lines)} lines from {logfile.path}')

    @property
    def handshake(self):
        if self._data_recs is None:
            self._data_recs = [data for data in HexDumpScanner(source=self.log_lines)]
            if self.env.verbose > 1:
                log.debug(f'detected {len(self._data_recs)} hexdumps '
                          f'in {self._logfile.path}')
        if self._hs_recs is None:
            self._hs_recs = [hrec for hrec in HandShake(source=self._data_recs,
                                                        verbose=self.env.verbose)]
            if self.env.verbose > 1:
                log.debug(f'detected {len(self._hs_recs)} crypto records '
                          f'in {self._logfile.path}')
        return self._hs_recs

    @property
    def hs_stripe(self):
        return ":".join([hrec.name for hrec in self.handshake])


def monitor_proc(env: Env, proc):
    _env = env
    proc.wait()


class ExampleServer:

    def __init__(self, env: Env, crypto_lib: str, verify_client=False):
        self.env = env
        self._crypto_lib = crypto_lib
        self._path = env.server_path(self._crypto_lib)
        self._logpath = f'{self.env.gen_dir}/{self._crypto_lib}-server.log'
        self._log = LogFile(path=self._logpath)
        self._logfile = None
        self._process = None
        self._verify_client = verify_client

    @property
    def path(self):
        return self._path

    @property
    def crypto_lib(self):
        return self._crypto_lib

    @property
    def uses_cipher_config(self):
        return CryptoLib.uses_cipher_config(self.crypto_lib)

    def supports_cipher(self, cipher):
        return CryptoLib.supports_cipher(self.crypto_lib, cipher)

    @property
    def log(self):
        return self._log

    def exists(self):
        return os.path.isfile(self.path)

    def start(self):
        if self._process is not None:
            return False
        creds = self.env.get_server_credentials()
        assert creds
        args = [
            self.path,
            f'--htdocs={self.env.htdocs_dir}',
        ]
        if self._verify_client:
            args.append('--verify-client')
        args.extend([
            '*', str(self.env.examples_port),
            creds.pkey_file, creds.cert_file
        ])
        self._logfile = open(self._logpath, 'w')
        self._process = subprocess.Popen(args=args, text=True,
                                         stdout=self._logfile, stderr=self._logfile)
        t = Thread(target=monitor_proc, daemon=True, args=(self.env, self._process))
        t.start()
        timeout = 5
        end = datetime.now() + timedelta(seconds=timeout)
        while True:
            if self._process.poll():
                return False
            try:
                if self.log.scan_recent(pattern=re.compile(r'^Using document root'), timeout=0.5):
                    break
            except TimeoutError:
                pass
            if datetime.now() > end:
                raise TimeoutError(f"pattern not found in error log after {timeout} seconds")
        self.log.advance()
        return True

    def stop(self):
        if self._process:
            self._process.terminate()
            self._process = None
        if self._logfile:
            self._logfile.close()
            self._logfile = None
        return True

    def restart(self):
        self.stop()
        self._log.reset()
        return self.start()

    def get_run(self) -> ServerRun:
        return ServerRun(env=self.env, logfile=self.log)
