// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 QPACK settings.
// Default dynamic table capacity is 4096 (implicit — H3 works)
// Default blocked streams is 100 (implicit — H3 works)
// Custom qpackMaxDTableCapacity overrides default
// Verifies that H3 sessions work with both default and custom QPACK
// settings. The defaults (4096 capacity, 100 blocked streams) are
// tested implicitly by all H3 tests. This test explicitly verifies
// custom values are accepted and functional.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const encoder = new TextEncoder();
const decoder = new TextDecoder();

async function makeRequest(clientSession, path) {
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': path,
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });
  const body = await bytes(stream);
  strictEqual(decoder.decode(body), path);
  await stream.closed;
}

// Custom qpackMaxDTableCapacity = 0 (disables dynamic table).
// QPACK compression still works via the static table, but the dynamic
// table is not used. Verifies the option is passed through to nghttp3.
{
  const serverDone = Promise.withResolvers();
  let requestCount = 0;

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(2);
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    // Server disables QPACK dynamic table.
    application: { qpackMaxDTableCapacity: 0, qpackBlockedStreams: 0 },
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode(headers[':path']));
      this.writer.endSync();
      if (++requestCount === 2) {
        serverDone.resolve();
      }
    }, 2),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    // Client also disables QPACK dynamic table.
    application: { qpackMaxDTableCapacity: 0, qpackBlockedStreams: 0 },
  });
  await clientSession.opened;

  // Multiple requests to exercise header compression paths.
  await makeRequest(clientSession, '/first');
  await makeRequest(clientSession, '/second');

  await serverDone.promise;
  clientSession.close();
}

// Custom qpackMaxDTableCapacity = 8192 (larger than default).
// Verifies large dynamic table capacity is accepted.
{
  const serverDone = Promise.withResolvers();
  let requestCount = 0;

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(2);
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    application: { qpackMaxDTableCapacity: 8192, qpackBlockedStreams: 200 },
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode(headers[':path']));
      this.writer.endSync();
      if (++requestCount === 2) {
        serverDone.resolve();
      }
    }, 2),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    application: { qpackMaxDTableCapacity: 8192, qpackBlockedStreams: 200 },
  });
  await clientSession.opened;

  await makeRequest(clientSession, '/alpha');
  await makeRequest(clientSession, '/beta');

  await serverDone.promise;
  clientSession.close();
}
