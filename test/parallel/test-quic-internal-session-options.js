// Flags: --expose-internals
'use strict';

require('../common');

const {
  constants,
  SessionOptions,
} = require('internal/quic/quic');

const test = require('node:test');

const {
  SocketAddress,
} = require('net');

const {
  webcrypto: {
    subtle,
  },
} = require('node:crypto');

async function getPrivateKey() {
  const {
    privateKey
  } = await subtle.generateKey({
    name: 'ECDSA',
    namedCurve: 'P-521',
  }, true, ['sign', 'verify']);
  return privateKey;
}

test('Creating a simple internalBinding(\'quic\'.SessionOptions works', async () => {
  new SessionOptions(constants.Side.SERVER, {
    alpn: constants.HTTP3_ALPN,
    secure: {
      key: await getPrivateKey(),
    }
  });
});

test('We can set some options', async () => {
  new SessionOptions(constants.Side.SERVER, {
    alpn: 'abc',
    servername: 'localhost',
    secure: {
      key: await getPrivateKey(),
    }
  });

  new SessionOptions(constants.Side.CLIENT, {
    alpn: 'abc',
    servername: 'localhost',
    preferredAddressStrategy: constants.PreferredAddressStrategy.USE,
    secure: {
      key: await getPrivateKey(),
    }
  });

  test('Setting TLS options works...', async () => {

    new SessionOptions(constants.Side.SERVER, {
      alpn: 'abc',
      servername: 'localhost',
      preferredAddressStrategy: constants.PreferredAddressStrategy.IGNORE,
      secure: {
        rejectUnauthorized: true,
        clientHello: false,
        enableTLSTrace: false,
        requestPeerCertificate: true,
        ocsp: false,
        verifyHostnameIdentity: true,
        keylog: false,
        sessionID: 'Hello World',
        ciphers: constants.DEFAULT_CIPHERS,
        groups: constants.DEFAULT_GROUPS,
        key: await getPrivateKey(),
        certs: [Buffer.from('hello')],
        ca: Buffer.from('what'),
        crl: Buffer.from('what is happening')
      }
    });
  });

  test('Setting Transport Param options works...', async () => {
    const ipv4 = new SocketAddress({ address: '123.123.123.123', family: 'ipv4' });
    const ipv6 = new SocketAddress({ address: '::1', family: 'ipv6' });

    new SessionOptions(constants.Side.CLIENT, {
      alpn: 'abc',
      secure: {
        key: await getPrivateKey(),
      },
      transportParams: {
        initialMaxStreamDataBidiLocal: 1,
        initialMaxStreamDataBidiRemote: 2,
        initialMaxStreamDataUni: 3,
        initialMaxData: 4,
        initialMaxStreamsBidi: 5,
        initialMaxStreamsUni: 6,
        maxIdleTimeout: 7,
        activeConnectionIdLimit: 8,
        ackDelayExponent: 9,
        maxAckDelay: 10,
        maxDatagramFrameSize: 11,
        disableActiveMigration: true,
      },
      preferredAddress: { ipv4, ipv6 }
    });
  });
});
