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
    /certificateCompression algorithm must be/
  );

  // Non-string entries should throw
  assert.throws(
    () => tls.createSecureContext({ certificateCompression: [1] }),
    /certificateCompression entries must be strings/
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
}

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
