// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Regression test for a heap-use-after-free in the QUIC session close path.
//
// When a server responds to an HTTP/3 request and then calls session.close()
// after the stream has fully drained (and been removed from the session's
// streams map), Session::Close(GRACEFUL) takes the "no open streams" branch
// and calls FinishClose(). FinishClose() emits the close event to JavaScript,
// and that synchronous JS callback can destroy the session — releasing the
// sole BaseObjectPtr<Session> held by SessionManager and freeing the Session
// object — while the SendPendingDataScope created at the top of Close() is
// still active. The scope's destructor then dereferences the freed session
// (SendPendingDataScope::~SendPendingDataScope() use-after-free).
//
// The fix pins the Session with a strong BaseObjectPtr<Session> for the
// lifetime of SendPendingDataScope (and of SendPendingData itself) so the
// destructor and trailing member accesses never operate on freed heap memory.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');
const decoder = new TextDecoder();
const encoder = new TextEncoder();

{
  let serverSession;
  let requestCount = 0;
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((ss) => {
    serverSession = ss;
    ss.closed.catch(() => {});
    ss.onstream = mustCall((stream) => {
      stream.closed.catch(() => {});
      stream.onheaders = mustCall((headers) => {
        const reqPath = headers[':path'] || '/';
        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync(encoder.encode('ok'));
        stream.writer.endSync();

        // Once the request stream has fully drained, close the session.
        // Deferring via setTimeout ensures the
        // stream has been removed from the session's streams map before
        // close() is called, so Close(GRACEFUL) immediately finishes the
        // close and emits the close event to JS synchronously while a
        // SendPendingDataScope is alive — the path that previously used a
        // freed Session.
        if (++requestCount === 1) {
          assert.strictEqual(reqPath, '/poc');
          stream.closed.then(mustCall(() => {
            setTimeout(mustCall(() => {
              if (!serverSession.destroyed) {
                serverSession.close();
              }
              serverDone.resolve();
            }), 50);
          })).catch(() => {});
        }
      });
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;
  clientSession.closed.catch(() => {});

  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/poc',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
  });

  const body = await bytes(stream);
  assert.strictEqual(decoder.decode(body), 'ok');
  await stream.closed;

  await serverDone.promise;
  await clientSession.closed.catch(() => {});

  await clientSession.close().catch(() => {});
  await serverEndpoint.close();
}
