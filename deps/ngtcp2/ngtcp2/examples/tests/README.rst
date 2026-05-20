Examples Tests
==============

This is a ``pytest`` suite intended to verify interoperability between
the different example clients and servers built.

You run it by executing ``pytest`` on top level project dir or in
the examples/tests directory.

.. code-block:: text

    examples/test> pytest
    ngtcp2-examples: [0.9.0-DEV, crypto_libs=['quictls', 'wolfssl']]
    ...

Requirements
------------

You need a Python3 (3.8 is probably sufficient), ``pytest`` and the
Python ``cryptography`` module installed.

Usage
-----

If you run ``pytest`` without arguments, it will print the test suite
and a ``.`` for every test case passed. Add ``-v`` and all test cases
will be listed in the full name. Adding several ``v`` will increase the
logging level on failure output.

The name of test cases include the crypto libs of the server and client
used. For example:

.. code-block:: text

    test_01_handshake.py::TestHandshake::test_01_01_get[quictls-quictls] PASSED                                                                                                  [ 16%]
    test_01_handshake.py::TestHandshake::test_01_01_get[quictls-wolfssl] PASSED

Here, ``test_01_01`` is run first with the quictls server and client and then
with the quictls server and wolfSSL client. By default, the test suite runs
all combinations of servers and clients that have been configured in the project.

To track down problems, you can restrict the test cases that are run by
matching patterns:

.. code-block:: text

    # only tests with wolfSSL example server
    > pytest -v -k 'wolfssl-'
    # only tests with wolfSSL example client
    > pytest -v -k 'test and -wolfssl'
    # tests with a specific combination
    > pytest -v -k 'quictls-wolfssl'


Analysing
---------

To make analysis of a broken test case easier, you best run only that
test case. Use ``pytest -vv`` (or more) to get more verbose logging.
Inspect server and client log files in ``examples/tests/gen``.
