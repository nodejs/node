import pytest

from .ngtcp2test import ExampleClient
from .ngtcp2test import ExampleServer
from .ngtcp2test import Env


@pytest.mark.skipif(condition=len(Env.get_crypto_libs()) == 0,
                    reason="no crypto lib examples configured")
class TestClientCert:

    @pytest.fixture(scope='class', params=Env.get_crypto_libs())
    def server(self, env, request) -> ExampleServer:
        s = ExampleServer(env=env, crypto_lib=request.param,
                          verify_client=True)
        assert s.exists(), f'server not found: {s.path}'
        assert s.start()
        yield s
        s.stop()

    @pytest.fixture(scope='function', params=Env.get_crypto_libs())
    def client(self, env, request) -> ExampleClient:
        client = ExampleClient(env=env, crypto_lib=request.param)
        assert client.exists()
        yield client

    def test_04_01(self, env: Env, server, client):
        # run GET with a server requesting a cert, client has none to offer
        cr = client.http_get(server, url=f'https://{env.example_domain}/')
        assert cr.returncode == 0
        cr.assert_verify_null_handshake()
        creqs = [r for r in cr.handshake if r.hsid == 13]  # CertificateRequest
        assert len(creqs) == 1
        creq = creqs[0].to_json()
        certs = [r for r in cr.server.handshake if r.hsid == 11]  # Certificate
        assert len(certs) == 1
        crec = certs[0].to_json()
        assert len(crec['certificate_list']) == 0
        assert creq['context'] == crec['context']
        # TODO: check that GET had no answer

    def test_04_02(self, env: Env, server, client):
        # run GET with a server requesting a cert, client has cert to offer
        credentials = env.ca.get_first("clientsX")
        cr = client.http_get(server, url=f'https://{env.example_domain}/',
                             credentials=credentials)
        assert cr.returncode == 0
        cr.assert_verify_cert_handshake()
        creqs = [r for r in cr.handshake if r.hsid == 13]  # CertificateRequest
        assert len(creqs) == 1
        creq = creqs[0].to_json()
        certs = [r for r in cr.server.handshake if r.hsid == 11]  # Certificate
        assert len(certs) == 1
        crec = certs[0].to_json()
        assert len(crec['certificate_list']) == 1
        assert creq['context'] == crec['context']
        # TODO: check that GET indeed gave a response
