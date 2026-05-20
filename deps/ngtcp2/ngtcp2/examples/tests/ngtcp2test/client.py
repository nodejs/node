import logging
import os
import re
import subprocess
from typing import List

import pytest

from .server import ExampleServer, ServerRun
from .certs import Credentials
from .tls import HandShake, HSRecord
from .env import Env, CryptoLib
from .log import LogFile, HexDumpScanner


log = logging.getLogger(__name__)


class ClientRun:

    def __init__(self, env: Env, returncode, logfile: LogFile, srun: ServerRun):
        self.env = env
        self.returncode = returncode
        self.logfile = logfile
        self.log_lines = logfile.get_recent()
        self._data_recs = None
        self._hs_recs = None
        self._srun = srun
        if self.env.verbose > 1:
            log.debug(f'read {len(self.log_lines)} lines from {logfile.path}')

    @property
    def handshake(self) -> List[HSRecord]:
        if self._data_recs is None:
            crypto_line =  re.compile(r'Ordered CRYPTO data in \S+ crypto level')
            scanner = HexDumpScanner(source=self.log_lines,
                                     leading_regex=crypto_line)
            self._data_recs = [data for data in scanner]
            if self.env.verbose > 1:
                log.debug(f'detected {len(self._data_recs)} crypto hexdumps '
                          f'in {self.logfile.path}')
        if self._hs_recs is None:
            self._hs_recs = [hrec for hrec in HandShake(source=self._data_recs,
                                                        verbose=self.env.verbose)]
            if self.env.verbose > 1:
                log.debug(f'detected {len(self._hs_recs)} crypto '
                          f'records in {self.logfile.path}')
        return self._hs_recs

    @property
    def hs_stripe(self) -> str:
        return ":".join([hrec.name for hrec in self.handshake])

    @property
    def early_data_rejected(self) -> bool:
        for l in self.log_lines:
            if re.match(r'^Early data was rejected by server.*', l):
                return True
        return False

    @property
    def server(self) -> ServerRun:
        return self._srun

    def norm_exp(self, c_hs, s_hs, allow_hello_retry=True):
        if allow_hello_retry and self.hs_stripe.startswith('HelloRetryRequest:'):
            c_hs = "HelloRetryRequest:" + c_hs
            s_hs = "ClientHello:" + s_hs
        return c_hs, s_hs

    def _assert_hs(self, c_hs, s_hs):
        if re.match('^'+c_hs, self.hs_stripe) is None:
            # what happened?
            if self.hs_stripe == '':
                # server send nothing
                if self.server.hs_stripe == '':
                    # client send nothing
                    pytest.fail(f'client did not send a ClientHello"')
                else:
                    # client send sth, but server did not respond
                    pytest.fail(f'server did not respond to ClientHello: '
                                f'{self.server.handshake[0].to_text()}"')
            else:
                pytest.fail(f'Expected "{c_hs}", got "{self.hs_stripe}"')
        assert re.match('^'+s_hs+'$', self.server.hs_stripe) is not None, \
            f'Expected "{s_hs}", got "{self.server.hs_stripe}"\n'

    def assert_non_resume_handshake(self, allow_hello_retry=True):
        # for client/server where KEY_SHARE do not match, the hello is retried
        c_hs, s_hs = self.norm_exp(
            "ServerHello:EncryptedExtensions:(Compressed)?Certificate:CertificateVerify:Finished",
            "ClientHello:Finished", allow_hello_retry=allow_hello_retry)
        self._assert_hs(c_hs, s_hs)

    def assert_resume_handshake(self):
        # for client/server where KEY_SHARE do not match, the hello is retried
        c_hs, s_hs = self.norm_exp("ServerHello:EncryptedExtensions:Finished",
                                   "ClientHello:Finished")
        self._assert_hs(c_hs, s_hs)

    def assert_verify_null_handshake(self):
        c_hs, s_hs = self.norm_exp(
            "ServerHello:EncryptedExtensions:CertificateRequest:(Compressed)?Certificate:CertificateVerify:Finished",
            "ClientHello:Certificate:Finished")
        self._assert_hs(c_hs, s_hs)

    def assert_verify_cert_handshake(self):
        c_hs, s_hs = self.norm_exp(
            "ServerHello:EncryptedExtensions:CertificateRequest:(Compressed)?Certificate:CertificateVerify:Finished",
            "ClientHello:Certificate:CertificateVerify:Finished")
        self._assert_hs(c_hs, s_hs)


class ExampleClient:

    def __init__(self, env: Env, crypto_lib: str):
        self.env = env
        self._crypto_lib = crypto_lib
        self._path = env.client_path(self._crypto_lib)
        self._log_path = f'{self.env.gen_dir}/{self._crypto_lib}-client.log'
        self._qlog_path = f'{self.env.gen_dir}/{self._crypto_lib}-client.qlog'
        self._session_path = f'{self.env.gen_dir}/{self._crypto_lib}-client.session'
        self._tp_path = f'{self.env.gen_dir}/{self._crypto_lib}-client.tp'
        self._data_path = f'{self.env.gen_dir}/{self._crypto_lib}-client.data'

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

    def exists(self):
        return os.path.isfile(self.path)

    def clear_session(self):
        if os.path.isfile(self._session_path):
            os.remove(self._session_path)
        if os.path.isfile(self._tp_path):
            os.remove(self._tp_path)

    def http_get(self, server: ExampleServer, url: str, extra_args: List[str] = None,
                 use_session=False, data=None,
                 credentials: Credentials = None,
                 ciphers: str = None):
        args = [
            self.path, '--exit-on-all-streams-close',
            f'--qlog-file={self._qlog_path}'
        ]
        if use_session:
            args.append(f'--session-file={self._session_path}')
            args.append(f'--tp-file={self._tp_path}')
        if data is not None:
            with open(self._data_path, 'w') as fd:
                fd.write(data)
            args.append(f'--data={self._data_path}')
        if ciphers is not None:
            ciphers = CryptoLib.adjust_ciphers(self.crypto_lib, ciphers)
            args.append(f'--ciphers={ciphers}')
        if credentials is not None:
            args.append(f'--key={credentials.pkey_file}')
            args.append(f'--cert={credentials.cert_file}')
        if extra_args is not None:
            args.extend(extra_args)
        args.extend([
            'localhost', str(self.env.examples_port),
            url
        ])
        if os.path.isfile(self._qlog_path):
            os.remove(self._qlog_path)
        with open(self._log_path, 'w') as log_file:
            logfile = LogFile(path=self._log_path)
            server.log.advance()
            process = subprocess.Popen(args=args, text=True,
                                       stdout=log_file, stderr=log_file)
            process.wait()
            return ClientRun(env=self.env, returncode=process.returncode,
                             logfile=logfile, srun=server.get_run())

