// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 close behavior.
// session.close() with open streams - streams complete cleanly
// Graceful H3 shutdown uses H3_NO_ERROR (0x100)

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:http3');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const decoder = new TextDecoder();

// Two streams. The graceful close waits for both streams to complete,
// then sends CONNECTION_CLOSE with H3_NO_ERROR.
{
  let serverSession;
  let requestCount = 0;
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    serverSession = ss;
    ss.onstream = mustCall((stream) => {
      stream.onheaders = mustCall((headers) => {
        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync(headers[':path']);
        stream.writer.endSync();

        // Close after both responses are written.
        if (++requestCount === 2) {
          serverSession.close();
          serverDone.resolve();
        }
      });
    }, 2);
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream1 = await clientSession.request({
    ':method': 'GET',
    ':path': '/one',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
  });

  const stream2 = await clientSession.request({
    ':method': 'GET',
    ':path': '/two',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
  });

  // Both streams should complete normally despite the close.
  const bodies = await Promise.all([bytes(stream1), bytes(stream2)]);
  strictEqual(decoder.decode(bodies[0]), '/one');
  strictEqual(decoder.decode(bodies[1]), '/two');

  await Promise.all([stream1.closed,
                     stream2.closed,
                     serverDone.promise,
                     clientSession.closed]);

  serverEndpoint.close();
}
