'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const net = require('net');
const { once } = require('events');
const { execFileSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const fixtures = require('../common/fixtures');

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
  tls.createSecureContext({
    key: fixtureKey, cert: fixtureCert, certificateCompression: ['zlib'],
  });
  tls.createSecureContext({
    key: fixtureKey, cert: fixtureCert, certificateCompression: ['brotli'],
  });
  tls.createSecureContext({
    key: fixtureKey, cert: fixtureCert, certificateCompression: ['zstd'],
  });

  // Valid multiple algorithms should not throw
  tls.createSecureContext({
    key: fixtureKey, cert: fixtureCert,
    certificateCompression: ['zlib', 'brotli', 'zstd'],
  });

  // Default (no certificateCompression) still works
  tls.createSecureContext({ key: fixtureKey, cert: fixtureCert });

  // Protocol range that excludes TLSv1.3 should throw - RFC 8879 is 1.3-only.
  assert.throws(
    () => tls.createSecureContext({
      key: fixtureKey, cert: fixtureCert,
      maxVersion: 'TLSv1.2',
      certificateCompression: ['zlib'],
    }),
    /TLSv1\.3/,
  );
}

// Test: certificate compression must not enable record-level compression.
//
// RFC 8879 certificate compression is unrelated to TLS record compression
// (the CRIME attack vector). The latter is disabled by Node at startup via
// sk_SSL_COMP_zero, and by modern OpenSSL defaults as well. To confirm, we
// Verify a ClientHello sent by Node only advertises null record compression.
(async () => {
  const { promise: clientHelloReceived, resolve: onClientHello } =
    Promise.withResolvers();
  const tcpServer = net.createServer((socket) => {
    socket.once('data', (chunk) => {
      onClientHello(chunk);
      socket.destroy();
    });
    socket.on('error', () => {});
  });
  tcpServer.listen(0);
  await once(tcpServer, 'listening');

  const probe = tls.connect({
    port: tcpServer.address().port,
    rejectUnauthorized: false,
    certificateCompression: ['zlib', 'brotli', 'zstd'],
  });
  probe.on('error', () => {});

  const clientHello = await clientHelloReceived;
  tcpServer.close();
  probe.destroy();

  // ClientHello layout (RFC 8446 4.1.2 + 5.1 record layer):
  //   record header (5) | handshake header (4) | legacy_version (2)
  //   | random (32) | session_id <1..32> | cipher_suites <2..>
  //   | compression_methods <1..> | extensions <2..>
  assert.strictEqual(clientHello[0], 0x16); // Handshake record
  assert.strictEqual(clientHello[5], 0x01); // ClientHello
  let i = 5 + 4 + 2 + 32;
  i += 1 + clientHello[i];                  // Skip legacy_session_id
  i += 2 + clientHello.readUInt16BE(i);     // Skip cipher_suites
  const methods = clientHello.subarray(i + 1, i + 1 + clientHello[i]);
  // Must only advertise the null compression method.
  assert.deepStrictEqual([...methods], [0x00]);
})().then(common.mustCall());

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
    certificateCompression: ['zlib'],
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
    certificateCompression: ['zlib'],
  });
  const [err] = await once(client, 'error');
  assert.strictEqual(err.code, 'ERR_SSL_EXCESSIVE_MESSAGE_SIZE');

  server.close();
})().then(common.mustCall());

// Test: TLS connection with certificate compression reduces handshake size.
//
// To see meaningful compression, we generate a certificate with many SANs to
// show easily testable differences. With a ~6 KB DER certificate, compression
// reduces the total handshake bytes by roughly 40-50% (but we assert 75%).
(async () => {
  // Generate a large self-signed certificate for testing.
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'tls-comp-'));
  const keyFile = path.join(tmpDir, 'key.pem');
  const certFile = path.join(tmpDir, 'cert.pem');

  const sans = [];
  for (let i = 0; i < 200; i++) {
    sans.push(`DNS:server${i}.example.com`);
  }

  execFileSync('openssl', [
    'req', '-new', '-x509', '-nodes', '-days', '1',
    '-newkey', 'rsa:2048',
    '-keyout', keyFile, '-out', certFile,
    '-subj', '/CN=test',
    '-addext', `subjectAltName=${sans.join(',')}`,
  ]);

  const key = fs.readFileSync(keyFile);
  const cert = fs.readFileSync(certFile);

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
  // during the TLS 1.3 handshake. With a ~6 KB certificate containing many
  // SANs, all three algorithms achieve ratios well below 0.75.
  for (const algo of ['zlib', 'brotli', 'zstd']) {
    const compressed = await measureHandshakeBytes(
      { key, cert, minVersion: 'TLSv1.3', certificateCompression: [algo] },
      { certificateCompression: [algo] },
    );
    const ratio = compressed / baseline;
    assert.ok(
      ratio < 0.75,
      `Expected ${algo} compressed handshake (${compressed} bytes, ` +
      `ratio=${ratio.toFixed(3)}) to be <75% of baseline ` +
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
          certificateCompression: ['zlib'],
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
        certificateCompression: ['zlib'],
      },
    );
    const sniRatio = sniBytes / baseline;
    assert.ok(
      sniRatio < 0.75,
      `Expected SNI compressed handshake (${sniBytes} bytes, ` +
      `ratio=${sniRatio.toFixed(3)}) to be <75% of baseline ` +
      `(${baseline} bytes)`
    );
  }

  fs.rmSync(tmpDir, { recursive: true });
})().then(common.mustCall());
