'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const { spawnSync } = require('child_process');
const tls = require('tls');
const net = require('net');
const { once } = require('events');
const fixtures = require('../common/fixtures');

if (process.argv[2] === 'record-compression') {
  (async () => {
    const tcpServer = net.createServer({ pauseOnConnect: true });
    tcpServer.listen(0);
    await once(tcpServer, 'listening');

    const clientConnected = once(tcpServer, 'connection');
    const probe = tls.connect({
      port: tcpServer.address().port,
      rejectUnauthorized: false,
      minVersion: 'TLSv1.2',
      maxVersion: 'TLSv1.2',
    });

    const [socket] = await clientConnected;
    const clientHelloReceived = once(socket, 'data');
    socket.resume();
    const [clientHello] = await clientHelloReceived;

    probe.destroy();
    socket.destroy();
    tcpServer.close();

    // ClientHello layout (RFC 5246 7.4.1.2):
    //   record header (5) | handshake header (4) | client_version (2)
    //   | random (32) | session_id <0..32> | cipher_suites <2..>
    //   | compression_methods <1..>
    assert.strictEqual(clientHello[0], 0x16);
    assert.strictEqual(clientHello[5], 0x01);
    let i = 5 + 4 + 2 + 32;
    i += 1 + clientHello[i];
    i += 2 + clientHello.readUInt16BE(i);
    const methods = clientHello.subarray(i + 1, i + 1 + clientHello[i]);
    assert.deepStrictEqual([...methods], [0x00]);
  })().then(common.mustCall());
  return;
}

// TLS record compression must remain disabled even when OpenSSL configuration
// tries to enable it at a security level that permits it.
{
  const config = fixtures.path('openssl-record-compression.cnf');
  const child = spawnSync(process.execPath, [
    `--openssl-config=${config}`,
    __filename,
    'record-compression',
  ], { encoding: 'utf8' });
  assert.strictEqual(child.status, 0, child.stderr);
}

const supportedAlgs = tls.getCertificateCompressionAlgorithms();
if (supportedAlgs.length === 0)
  common.skip('certificate compression not supported by this OpenSSL build');

// Use small fixture certs for input validation tests.
const fixtureKey = fixtures.readKey('agent1-key.pem');
const fixtureCert = fixtures.readKey('agent1-cert.pem');

// Test: input validation
{
  // Empty array is valid (means disabled, same as undefined)
  tls.createSecureContext({
    key: fixtureKey, cert: fixtureCert, certificateCompression: [],
  });

  // Non-array should throw
  assert.throws(
    () => tls.createSecureContext({ certificateCompression: true }),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );

  assert.throws(
    () => tls.createSecureContext({ certificateCompression: 'zlib' }),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );

  // Invalid algorithm name should throw
  assert.throws(
    () => tls.createSecureContext({ certificateCompression: ['invalid'] }),
    /must be 'zlib', 'brotli', or 'zstd'/
  );

  // Non-string entries should throw
  assert.throws(
    () => tls.createSecureContext({ certificateCompression: [1] }),
    /must be 'zlib', 'brotli', or 'zstd'/
  );

  // Valid single algorithms should not throw
  for (const algo of supportedAlgs) {
    tls.createSecureContext({
      key: fixtureKey, cert: fixtureCert, certificateCompression: [algo],
    });
  }

  // Valid multiple algorithms should not throw
  tls.createSecureContext({
    key: fixtureKey, cert: fixtureCert,
    certificateCompression: supportedAlgs,
  });

  // Default (no certificateCompression) still works
  tls.createSecureContext({ key: fixtureKey, cert: fixtureCert });

  // Protocol range that excludes TLSv1.3 should throw - RFC 8879 is 1.3-only.
  assert.throws(
    () => tls.createSecureContext({
      key: fixtureKey, cert: fixtureCert,
      maxVersion: 'TLSv1.2',
      certificateCompression: [supportedAlgs[0]],
    }),
    /TLSv1\.3/,
  );
}

// Test: a CompressedCertificate whose uncompressed length exceeds OpenSSL's
// default max_cert_list (100 KB) is rejected before decompression, protecting
// against decompression bombs in malicious server's cert chains.
(async () => {
  // Cert just repeats our existing fixture many times
  const bigChain = Buffer.concat(
    Array(200).fill(Buffer.from(fixtureCert))
  );

  const server = tls.createServer({
    key: fixtureKey, cert: bigChain,
    minVersion: 'TLSv1.3',
    certificateCompression: [supportedAlgs[0]],
  }, common.mustNotCall());

  // The aborted handshake surfaces as a tlsClientError on the server too.
  server.on('tlsClientError', () => {});
  server.listen(0);
  await once(server, 'listening');

  // Client starts handshake, server sends enormous cert, client rejects:
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
    minVersion: 'TLSv1.3',
    certificateCompression: [supportedAlgs[0]],
  });
  const [err] = await once(client, 'error');
  assert.strictEqual(err.code, 'ERR_SSL_EXCESSIVE_MESSAGE_SIZE');

  server.close();
})().then(common.mustCall());

// Test: TLS connection with certificate compression reduces handshake size.
(async () => {
  const key = fixtureKey;

  // Include a massive certificate list. Doesn't matter that they're not a valid chain,
  // we'll send them all and the client uses rejectUnauthorized: false.
  const cert = Buffer.concat(Array(20).fill(Buffer.from(fixtureCert)));

  // Helper: perform a TLS 1.3 handshake via a TCP proxy and return the total
  // raw bytes transferred. The proxy counts bytes to measure the on-the-wire
  // handshake size (which reflects certificate compression savings).
  async function measureHandshakeBytes(serverOpts, clientOpts) {
    const { promise: serverConnected, resolve: onServerConn } = Promise.withResolvers();
    const server = tls.createServer(serverOpts, common.mustCall((socket) => {
      onServerConn();
      socket.destroy();
    }));
    server.listen(0);
    await once(server, 'listening');

    let proxyClientConn;
    const proxy = net.createServer(common.mustCall((clientConn) => {
      proxyClientConn = clientConn;
      const serverConn = net.connect(server.address().port);
      clientConn.pipe(serverConn);
      serverConn.pipe(clientConn);
      serverConn.on('error', () => {});
      clientConn.on('error', () => {});
    }));
    proxy.listen(0);
    await once(proxy, 'listening');

    const client = tls.connect({
      port: proxy.address().port,
      rejectUnauthorized: false,
      minVersion: 'TLSv1.3',
      ...clientOpts,
    });
    await serverConnected;

    const totalBytes = proxyClientConn.bytesRead + proxyClientConn.bytesWritten;

    client.destroy();
    server.close();
    proxy.close();

    return totalBytes;
  }

  const baseline = await measureHandshakeBytes(
    { key, cert, minVersion: 'TLSv1.3' },
    {},
  );

  // Test each compression algorithm produces a measurably smaller handshake.
  // Certificate compression (RFC 8879) compresses the Certificate message
  // during the TLS 1.3 handshake. With the large repeated cert list above, all
  // supported algorithms achieve ratios well below 0.5.
  for (const algo of supportedAlgs) {
    const compressed = await measureHandshakeBytes(
      { key, cert, minVersion: 'TLSv1.3', certificateCompression: [algo] },
      { certificateCompression: [algo] },
    );
    const ratio = compressed / baseline;
    assert.ok(
      ratio < 0.5,
      `Expected ${algo} compressed handshake (${compressed} bytes, ` +
      `ratio=${ratio.toFixed(3)}) to be <50% of baseline ` +
      `(${baseline} bytes)`
    );
  }

  // Test: certificate compression works through SNI callback.
  // The server uses a default context without compression, and an SNI context
  // with compression enabled. The client connects with an SNI hostname that
  // triggers the SNI callback, switching to the compressed context.
  {
    const sniCalled = common.mustCall((hostname, cb) => {
      if (hostname === 'compressed.example.com') {
        const ctx = tls.createSecureContext({
          key, cert,
          certificateCompression: [supportedAlgs[0]],
        });
        cb(null, ctx);
      } else {
        cb(null);
      }
    });

    const sniBytes = await measureHandshakeBytes(
      {
        key: fixtureKey, cert: fixtureCert,
        minVersion: 'TLSv1.3',
        SNICallback: sniCalled,
      },
      {
        servername: 'compressed.example.com',
        certificateCompression: [supportedAlgs[0]],
      },
    );
    const sniRatio = sniBytes / baseline;
    assert.ok(
      sniRatio < 0.5,
      `Expected SNI compressed handshake (${sniBytes} bytes, ` +
      `ratio=${sniRatio.toFixed(3)}) to be <50% of baseline ` +
      `(${baseline} bytes)`
    );
  }
})().then(common.mustCall());
