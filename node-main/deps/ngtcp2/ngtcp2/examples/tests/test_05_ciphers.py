import sys

import pytest

from .ngtcp2test import ExampleClient
from .ngtcp2test import ExampleServer
from .ngtcp2test import Env


@pytest.mark.skipif(condition=len(Env.get_crypto_libs()) == 0,
                    reason="no crypto lib examples configured")
class TestCiphers:

    @pytest.fixture(scope='class', params=Env.get_crypto_libs())
    def server(self, env, request) -> ExampleServer:
        s = ExampleServer(env=env, crypto_lib=request.param)
        assert s.exists(), f'server not found: {s.path}'
        assert s.start()
        yield s
        s.stop()

    @pytest.fixture(scope='function',
                    params=Env.get_crypto_libs(configurable_ciphers=True))
    def client(self, env, request) -> ExampleClient:
        client = ExampleClient(env=env, crypto_lib=request.param)
        assert client.exists()
        yield client

    @pytest.mark.parametrize('cipher', [
        'TLS_AES_128_GCM_SHA256',
        'TLS_AES_256_GCM_SHA384',
        'TLS_CHACHA20_POLY1305_SHA256',
        'TLS_AES_128_CCM_SHA256',
    ])
    def test_05_01_get(self, env: Env, server, client, cipher):
        if not client.uses_cipher_config:
            pytest.skip(f'client {client.crypto_lib} ignores cipher config\n')
        # run simple GET, no sessions, needs to give full handshake
        if not client.supports_cipher(cipher):
            pytest.skip(f'client {client.crypto_lib} does not support {cipher}\n')
        if not server.supports_cipher(cipher):
            pytest.skip(f'server {server.crypto_lib} does not support {cipher}\n')
        cr = client.http_get(server, url=f'https://{env.example_domain}/',
                             ciphers=cipher)
        assert cr.returncode == 0
        cr.assert_non_resume_handshake()
