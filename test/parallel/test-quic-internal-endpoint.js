// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');

const {
  constants,
  EndpointOptions,
  SessionOptions,
  Endpoint,
  ArrayBufferViewSource,
  BlobSource,
} = require('internal/quic/quic');

const {
  Blob,
} = require('buffer');

const {
  kHandle,
} = require('internal/blob');

const test = require('node:test');

const {
  SocketAddress
} = require('net');

const {
  createPrivateKey,
  webcrypto: {
    subtle,
  },
} = require('node:crypto');

const {
  strictEqual,
} = require('assert');

const fixtures = require('../common/fixtures');

async function getPrivateKey() {
  const {
    privateKey
  } = await subtle.generateKey({
    name: 'ECDSA',
    namedCurve: 'P-521',
  }, true, ['sign', 'verify']);
  return privateKey;
}

const key = createPrivateKey(fixtures.readKey('rsa_private.pem'));
const ca = [fixtures.readKey('rsa_ca.crt')];
const certs = fixtures.readKey('rsa_cert.crt');

test('We can create a local unused endpoint', async () => {
  const options = new EndpointOptions(new SocketAddress());
  const endpoint = new Endpoint(options);

  strictEqual(endpoint.stats.destroyedAt, 0n);
  strictEqual(endpoint.stats.bytesReceived, 0n);
  strictEqual(endpoint.stats.bytesSent, 0n);
  strictEqual(endpoint.stats.packetsReceived, 0n);
  strictEqual(endpoint.stats.packetsSent, 0n);
  strictEqual(endpoint.stats.serverSessions, 0n);
  strictEqual(endpoint.stats.clientSessions, 0n);
  strictEqual(endpoint.stats.busyCount, 0n);

  // If the endpoint is unused, it should not hold the event loop open or have any errors.
});

test('Create an unref\'d listening endpoint', async () => {
  const endpointOptions = new EndpointOptions(new SocketAddress());
  const endpoint = new Endpoint(endpointOptions);

  endpoint.listen(new SessionOptions(constants.Side.SERVER, {
    alpn: 'zzz',
    secure: {
      key: await getPrivateKey(),
    }
  })).unref();

  // The listening endpoint should not keep the event loop open and
  // exiting should not cause any crashes.
});

test('Create a listening endpoint', async () => {
  const endpointOptions = new EndpointOptions(new SocketAddress());
  const endpoint = new Endpoint(endpointOptions);

  endpoint.listen(new SessionOptions(constants.Side.SERVER, {
    alpn: 'zzz',
    secure: {
      key: await getPrivateKey(),
    }
  }));

  endpoint.markAsBusy();
  endpoint.markAsBusy(false);

  const address = endpoint.address;

  strictEqual(address.address, '127.0.0.1');
  strictEqual(address.family, 'ipv4');

  endpoint.addEventListener('close', common.mustCall());

  // Stop listening and gracefully close.
  await endpoint.close();

  // The stats are still present, but detached.
  strictEqual(endpoint.stats.detached, true);
  strictEqual(endpoint.stats.bytesReceived, 0n);
  strictEqual(endpoint.stats.bytesSent, 0n);
  strictEqual(endpoint.stats.packetsReceived, 0n);
  strictEqual(endpoint.stats.packetsSent, 0n);
  strictEqual(endpoint.stats.serverSessions, 0n);
  strictEqual(endpoint.stats.clientSessions, 0n);
  strictEqual(endpoint.stats.busyCount, 0n);
});

test('Create a client session', async () => {
  const endpointOptions = new EndpointOptions(new SocketAddress());
  const endpoint = new Endpoint(endpointOptions);
  const client = endpoint.connect(
    new SocketAddress(),
    new SessionOptions(constants.Side.CLIENT, {
      alpn: 'zzz',
      transportParams: {
        maxIdleTimeout: 1
      }
    }));

  client.addEventListener('close', () => endpoint.close());
});

test('Client and server', async () => {
  const serverOptions = new EndpointOptions(new SocketAddress({}), {});
  const server = new Endpoint(serverOptions);
  server.listen(new SessionOptions(constants.Side.SERVER, {
    alpn: 'zzz',
    secure: {
      key,
      certs,
      ca,
      clientHello: true,
    },
  }));

  server.addEventListener('session', ({ session }) => {

    session.onclienthello = (event) => {
      console.log(event.alpn);
      console.log(event.servername);
      console.log(event.ciphers);
      event.done();
    };

    session.addEventListener('handshake-complete', () => {
      console.log('Server handshake is complete', session.handshakeConfirmed);
    });

    session.addEventListener('close', () => {
      console.log('Server session closed');
    });
    session.addEventListener('error', ({ error }) => {
      console.log('Server session error', error);
    });
    session.addEventListener('stream', ({ stream }) => {
      stream.addEventListener('data', ({ chunks }) => {
        console.log(chunks);
      });

      stream.attachSource(new BlobSource((new Blob(['world']))[kHandle]));
    });
    session.addEventListener('datagram', console.log);
    session.addEventListener('ocsp', console.log);
    session.addEventListener('path-validation', console.log);
  });

  console.log(server.address);

  // ---------
  const clientOptions = new EndpointOptions(new SocketAddress({}));
  const client = new Endpoint(clientOptions);

  const session = client.connect(
    server.address,
    new SessionOptions(constants.Side.CLIENT, {
      alpn: 'zzz',
      servername: 'localhost',
      secure: {
        rejectUnauthorized: false,
        enableTLSTrace: true,
        verifyHostnameIdentity: false,
      }
    }));

  session.addEventListener('close', () => {
    console.log('Client session closed');
    client.close();
    server.close();
  });
  session.addEventListener('error', ({ error }) => {
    console.log('Client session error', error);
  });
  session.addEventListener('stream', console.log);
  session.addEventListener('datagram', console.log);
  session.addEventListener('ocsp', console.log);

  session.addEventListener('path-validation', console.log);

  session.onhandshakecomplete = () => {
    console.log('Client handshake is complete', session.handshakeConfirmed);
  };

  session.onsessionticket = ({ ticket }) => {
    console.log('Updated session ticket received', ticket);
  };

  await session.handshakeCompleted;

  const stream = session.open();

  const enc = new TextEncoder();
  const source = new ArrayBufferViewSource(enc.encode('hello'));
  stream.attachSource(source);

  stream.onclose = () => console.log('stream closed!');
  stream.onerror = ({ error }) => console.log('stream error', error);
  stream.ondata = ({ chunks, ended }) => {
    console.log(chunks);
    console.log(ended);
  };
});
