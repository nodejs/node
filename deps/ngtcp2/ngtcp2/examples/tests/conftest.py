import logging
import pytest

from .ngtcp2test import Env


@pytest.mark.usefixtures("env")
def pytest_report_header(config):
    env = Env()
    return [
        f"ngtcp2-examples: [{env.version}, crypto_libs={env.crypto_libs}]",
        f"example clients: {env.clients}",
        f"example servers: {env.servers}",
    ]


@pytest.fixture(scope="package")
def env(pytestconfig) -> Env:
    console = logging.StreamHandler()
    console.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
    logging.getLogger('').addHandler(console)
    env = Env(pytestconfig=pytestconfig)
    level = logging.DEBUG if env.verbose > 0 else logging.INFO
    console.setLevel(level)
    logging.getLogger('').setLevel(level=level)
    env.setup()

    return env
