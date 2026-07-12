import pytest

from .ngtcp2test import ExampleClient
from .ngtcp2test import ExampleServer
from .ngtcp2test import Env


@pytest.mark.skipif(condition=len(Env.get_crypto_libs()) == 0,
                    reason="no crypto lib examples configured")
class TestEarlyData:

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

    def test_03_01(self, env: Env, server, client):
        # run GET with sessions, cleared first, without a session, early
        # data will not even be attempted
        client.clear_session()
        edata = 'This is the early data. It is not much.'
        cr = client.http_get(server, url=f'https://{env.example_domain}/',
                             use_session=True, data=edata)
        assert cr.returncode == 0
        cr.assert_non_resume_handshake()
        # resume session, early data is sent and accepted
        cr = client.http_get(server, url=f'https://{env.example_domain}/',
                             use_session=True, data=edata)
        assert cr.returncode == 0
        cr.assert_resume_handshake()
        assert not cr.early_data_rejected
        # restart the server, resume, early data is attempted but will not work
        server.restart()
        cr = client.http_get(server, url=f'https://{env.example_domain}/',
                             use_session=True, data=edata)
        assert cr.returncode == 0
        assert cr.early_data_rejected
        cr.assert_non_resume_handshake()
        # restart again, sent data, but not as early data
        server.restart()
        cr = client.http_get(server, url=f'https://{env.example_domain}/',
                             use_session=True, data=edata,
                             extra_args=['--disable-early-data'])
        assert cr.returncode == 0
        # we see no rejection, since it was not used
        assert not cr.early_data_rejected
        cr.assert_non_resume_handshake()
