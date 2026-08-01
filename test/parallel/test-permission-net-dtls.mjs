// Flags: --permission --allow-fs-read=* --experimental-dtls --no-warnings
import { hasCrypto, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen, DTLSEndpoint } = await import('node:dtls');

// Verify that the permission system correctly reports no net access.
assert.ok(!process.permission.has('net'));

const __dirname = dirname(fileURLToPath(import.meta.url));
const fixturesDir = join(__dirname, '..', 'fixtures', 'keys');
const cert = readFileSync(join(fixturesDir, 'agent1-cert.pem')).toString();
const key = readFileSync(join(fixturesDir, 'agent1-key.pem')).toString();
const ca = readFileSync(join(fixturesDir, 'ca1-cert.pem')).toString();

// Test: connect() should throw ERR_ACCESS_DENIED
{
  assert.throws(
    () => connect('127.0.0.1', 12345, { ca: [ca], rejectUnauthorized: false }),
    {
      code: 'ERR_ACCESS_DENIED',
      permission: 'Net',
    },
  );
}

// Test: listen() should throw ERR_ACCESS_DENIED
{
  assert.throws(
    () => listen(mustNotCall('onsession should not be called'), {
      cert,
      key,
      port: 0,
      host: '127.0.0.1',
    }),
    {
      code: 'ERR_ACCESS_DENIED',
      permission: 'Net',
    },
  );
}

// Test: Creating a DTLSEndpoint is allowed (no network I/O at construction
// time), but bind() requires net permission.
{
  const endpoint = new DTLSEndpoint();
  assert.ok(endpoint);
  assert.throws(
    () => endpoint.bind('127.0.0.1', 0),
    {
      code: 'ERR_ACCESS_DENIED',
      permission: 'Net',
    },
  );
}
