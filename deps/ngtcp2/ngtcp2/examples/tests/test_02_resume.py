import pytest

from .ngtcp2test import ExampleClient
from .ngtcp2test import ExampleServer
from .ngtcp2test import Env


@pytest.mark.skipif(condition=len(Env.get_crypto_libs()) == 0,
                    reason="no crypto lib examples configured")
class TestResume:

    @pytest.fixture(scope='class', params=Env.get_crypto_libs())
    def server(self, env, request) -> ExampleServer:
        s = ExampleServer(env=env, crypto_lib=request.param)
        assert s.exists(), f'server not found: {s.path}'
        assert s.start()
        yield s
        s.stop()

    @pytest.fixture(scope='function', params=Env.get_crypto_libs())
    def client(self, env, request) -> ExampleClient:
        client = ExampleClient(env=env, crypto_lib=request.param)
        assert client.exists()
        yield client

    def test_02_01(self, env: Env, server, client):
        # run GET with sessions but no early data, cleared first, then reused
        client.clear_session()
        cr = client.http_get(server, url=f'https://{env.example_domain}/',
                             use_session=True,
                             extra_args=['--disable-early-data'])
        assert cr.returncode == 0
        cr.assert_non_resume_handshake()
        # Now do this again and we expect a resumption, meaning no certificate
        cr = client.http_get(server, url=f'https://{env.example_domain}/',
                             use_session=True,
                             extra_args=['--disable-early-data'])
        assert cr.returncode == 0
        cr.assert_resume_handshake()
        # restart the server, do it again
        server.restart()
        cr = client.http_get(server, url=f'https://{env.example_domain}/',
                             use_session=True,
                             extra_args=['--disable-early-data'])
        assert cr.returncode == 0
        cr.assert_non_resume_handshake()
