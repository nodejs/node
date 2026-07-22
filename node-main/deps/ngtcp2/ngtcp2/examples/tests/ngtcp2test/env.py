import logging
import os
from configparser import ConfigParser, ExtendedInterpolation
from typing import Dict, Optional

from .certs import CertificateSpec, Ngtcp2TestCA, Credentials

log = logging.getLogger(__name__)


class CryptoLib:

    IGNORES_CIPHER_CONFIG = [
        'picotls', 'boringssl'
    ]
    UNSUPPORTED_CIPHERS = {
        'wolfssl': [
            'TLS_AES_128_CCM_SHA256',  # no plans to
        ],
        'picotls': [
            'TLS_AES_128_CCM_SHA256',  # no plans to
        ],
        'boringssl': [
            'TLS_AES_128_CCM_SHA256',  # no plans to
        ]
    }
    GNUTLS_CIPHERS = {
        'TLS_AES_128_GCM_SHA256': 'AES-128-GCM',
        'TLS_AES_256_GCM_SHA384': 'AES-256-GCM',
        'TLS_CHACHA20_POLY1305_SHA256': 'CHACHA20-POLY1305',
        'TLS_AES_128_CCM_SHA256': 'AES-128-CCM',
    }

    @classmethod
    def uses_cipher_config(cls, crypto_lib):
        return crypto_lib not in cls.IGNORES_CIPHER_CONFIG

    @classmethod
    def supports_cipher(cls, crypto_lib, cipher):
        return crypto_lib not in cls.UNSUPPORTED_CIPHERS or \
               cipher not in cls.UNSUPPORTED_CIPHERS[crypto_lib]

    @classmethod
    def adjust_ciphers(cls, crypto_lib, ciphers: str) -> str:
        if crypto_lib == 'gnutls':
            gciphers = "NORMAL:-VERS-ALL:+VERS-TLS1.3:-CIPHER-ALL"
            for cipher in ciphers.split(':'):
                gciphers += f':+{cls.GNUTLS_CIPHERS[cipher]}'
            return gciphers
        return ciphers


def init_config_from(conf_path):
    if os.path.isfile(conf_path):
        config = ConfigParser(interpolation=ExtendedInterpolation())
        config.read(conf_path)
        return config
    return None


TESTS_PATH = os.path.dirname(os.path.dirname(__file__))
EXAMPLES_PATH = os.path.dirname(TESTS_PATH)
DEF_CONFIG = init_config_from(os.path.join(TESTS_PATH, 'config.ini'))


class Env:

    @classmethod
    def get_crypto_libs(cls, configurable_ciphers=None):
        names = [name for name in DEF_CONFIG['examples']
                 if DEF_CONFIG['examples'][name] == 'yes']
        if configurable_ciphers is not None:
            names = [n for n in names if CryptoLib.uses_cipher_config(n)]
        return names

    def __init__(self, examples_dir=None, tests_dir=None, config=None,
                 pytestconfig=None):
        self._verbose = pytestconfig.option.verbose if pytestconfig is not None else 0
        self._examples_dir = examples_dir if examples_dir is not None else EXAMPLES_PATH
        self._tests_dir = examples_dir if tests_dir is not None else TESTS_PATH
        self._gen_dir = os.path.join(self._tests_dir, 'gen')
        self.config = config if config is not None else DEF_CONFIG
        self._version = self.config['ngtcp2']['version']
        self._crypto_libs = [name for name in self.config['examples']
                             if self.config['examples'][name] == 'yes']
        self._clients = [self.config['clients'][lib] for lib in self._crypto_libs
                         if lib in self.config['clients']]
        self._servers = [self.config['servers'][lib] for lib in self._crypto_libs
                         if lib in self.config['servers']]
        self._examples_pem = {
            'key': 'xxx',
            'cert': 'xxx',
        }
        self._htdocs_dir = os.path.join(self._gen_dir, 'htdocs')
        self._tld = 'tests.ngtcp2.nghttp2.org'
        self._example_domain = f"one.{self._tld}"
        self._ca = None
        self._cert_specs = [
            CertificateSpec(domains=[self._example_domain], key_type='rsa2048'),
            CertificateSpec(name="clientsX", sub_specs=[
               CertificateSpec(name="user1", client=True),
            ]),
        ]

    def issue_certs(self):
        if self._ca is None:
            self._ca = Ngtcp2TestCA.create_root(name=self._tld,
                                               store_dir=os.path.join(self.gen_dir, 'ca'),
                                               key_type="rsa2048")
        self._ca.issue_certs(self._cert_specs)

    def setup(self):
        os.makedirs(self._gen_dir, exist_ok=True)
        os.makedirs(self._htdocs_dir, exist_ok=True)
        self.issue_certs()

    def get_server_credentials(self) -> Optional[Credentials]:
        creds = self.ca.get_credentials_for_name(self._example_domain)
        if len(creds) > 0:
            return creds[0]
        return None

    @property
    def verbose(self) -> int:
        return self._verbose

    @property
    def version(self) -> str:
        return self._version

    @property
    def gen_dir(self) -> str:
        return self._gen_dir

    @property
    def ca(self):
        return self._ca

    @property
    def htdocs_dir(self) -> str:
        return self._htdocs_dir

    @property
    def example_domain(self) -> str:
        return self._example_domain

    @property
    def examples_dir(self) -> str:
        return self._examples_dir

    @property
    def examples_port(self) -> int:
        return int(self.config['examples']['port'])

    @property
    def examples_pem(self) -> Dict[str, str]:
        return self._examples_pem

    @property
    def crypto_libs(self):
        return self._crypto_libs

    @property
    def clients(self):
        return self._clients

    @property
    def servers(self):
        return self._servers

    def client_name(self, crypto_lib):
        if crypto_lib in self.config['clients']:
            return self.config['clients'][crypto_lib]
        return None

    def client_path(self, crypto_lib):
        cname = self.client_name(crypto_lib)
        if cname is not None:
            return os.path.join(self.examples_dir, cname)
        return None

    def server_name(self, crypto_lib):
        if crypto_lib in self.config['servers']:
            return self.config['servers'][crypto_lib]
        return None

    def server_path(self, crypto_lib):
        sname = self.server_name(crypto_lib)
        if sname is not None:
            return os.path.join(self.examples_dir, sname)
        return None
