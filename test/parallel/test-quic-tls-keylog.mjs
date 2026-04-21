// Flags: --experimental-quic --no-warnings

// Test: keylog callback.
// When keylog: true, TLS key material is delivered to the
// session.onkeylog callback during the handshake for both
// client and server sessions.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const clientLines = [];
const serverLines = [];

const expectedLabels = [
  'CLIENT_HANDSHAKE_TRAFFIC_SECRET',
  'SERVER_HANDSHAKE_TRAFFIC_SECRET',
  'CLIENT_TRAFFIC_SECRET_0',
  'SERVER_TRAFFIC_SECRET_0',
];

function assertKeylogLines(lines, side) {
  ok(lines.length > 0, `Expected ${side} keylog lines, got ${lines.length}`);

  for (const line of lines) {
    strictEqual(typeof line, 'string',
                `Each ${side} keylog line should be a string`);
  }

  const joined = lines.join('');
  for (const label of expectedLabels) {
    ok(joined.includes(label),
       `Expected ${side} keylog to contain ${label}`);
  }
}

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  serverSession.close();
}), {
  keylog: true,
  onkeylog(line) {
    serverLines.push(line);
  },
});

const clientSession = await connect(serverEndpoint.address, {
  keylog: true,
  onkeylog(line) {
    clientLines.push(line);
  },
});

await clientSession.opened;
await clientSession.closed;
await serverEndpoint.close();

assertKeylogLines(clientLines, 'client');
assertKeylogLines(serverLines, 'server');
