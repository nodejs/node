// Flags: --permission --allow-fs-read=* --experimental-quic --no-warnings
import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { createPrivateKey } = await import('node:crypto');
const { connect, listen, QuicEndpoint } = await import('node:quic');

// Verify that the permission system correctly reports no net access.
assert.ok(!process.permission.has('net'));

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// Test: connect() should reject with ERR_ACCESS_DENIED
{
  await assert.rejects(
    connect('127.0.0.1:12345', { alpn: 'h3' }),
    {
      code: 'ERR_ACCESS_DENIED',
      permission: 'Net',
    },
  );
}

// Test: listen() should throw ERR_ACCESS_DENIED
{
  await assert.rejects(
    listen(mustNotCall('onsession should not be called'), {
      alpn: ['h3'],
      sni: { '*': { keys: [key], certs: [cert] } },
    }),
    {
      code: 'ERR_ACCESS_DENIED',
      permission: 'Net',
    },
  );
}

// Test: Creating a QuicEndpoint without connect/listen is allowed
// since no network I/O occurs at construction time.
{
  const endpoint = new QuicEndpoint();
  // The endpoint exists but has not performed any network operations.
  await endpoint.close();
}
