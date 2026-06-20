// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 callback error handling.
// Sync throw in onorigin callback destroys the session
// Sync throw in onheaders callback destroys the stream
// Async rejection in onheaders callback destroys the stream
// Sync throw in ontrailers callback destroys the stream
// Sync throw in onwanttrailers callback destroys the stream

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, rejects } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:http3');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const encoder = new TextEncoder();

async function makeServer(onheadersHandler, extraCallbacks = {}) {
  const done = Promise.withResolvers();
  const ep = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall((stream) => {
      stream.onheaders = (headers) => onheadersHandler(stream, headers);
      if (extraCallbacks.onwanttrailers) {
        stream.onwanttrailers = () => extraCallbacks.onwanttrailers(stream);
      }
      // The server completes its response before the client's
      // callback throws, so the server stream always resolves.
      stream.closed.then(mustCall());
    });
    await ss.closed;
    done.resolve();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    transportParams: { maxIdleTimeout: 1 },
  });
  return { ep, done };
}

// Sync throw in onheaders callback destroys the stream.
{
  const { ep, done } = await makeServer(
    mustCall((stream, headers) => {
      stream.sendHeaders({ ':status': '200' });
      stream.writer.writeSync(encoder.encode('ok'));
      stream.writer.endSync();
    }),
  );

  const c = await connect(ep.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });
  await c.opened;

  const s = await c.request({
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall(() => {
      throw new Error('onheaders sync error');
    }),
  });

  await rejects(s.closed, mustCall((err) => {
    strictEqual(err.message, 'onheaders sync error');
    return true;
  }));
  strictEqual(s.destroyed, true);

  c.close();
  await done.promise;
  ep.close();
}

// Async rejection in onheaders callback destroys the stream.
{
  const { ep, done } = await makeServer(
    mustCall((stream, headers) => {
      stream.sendHeaders({ ':status': '200' });
      stream.writer.writeSync(encoder.encode('ok'));
      stream.writer.endSync();
    }),
  );

  const c = await connect(ep.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });
  await c.opened;

  const s = await c.request({
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall(async () => {
      throw new Error('onheaders async error');
    }),
  });

  await rejects(s.closed, mustCall((err) => {
    strictEqual(err.message, 'onheaders async error');
    return true;
  }));
  strictEqual(s.destroyed, true);

  c.close();
  await done.promise;
  ep.close();
}

// Sync throw in ontrailers callback destroys the stream.
{
  const { ep, done } = await makeServer(
    mustCall((stream, headers) => {
      stream.sendHeaders({ ':status': '200' });
      stream.writer.writeSync(encoder.encode('body'));
      stream.writer.endSync();
    }),
    {
      onwanttrailers: mustCall((stream) => {
        stream.sendTrailers({ 'x-trailer': 'value' });
      }),
    },
  );

  const c = await connect(ep.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });
  await c.opened;

  const s = await c.request({
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
    ontrailers: mustCall(() => {
      throw new Error('ontrailers sync error');
    }),
  });

  await rejects(s.closed, mustCall((err) => {
    strictEqual(err.message, 'ontrailers sync error');
    return true;
  }));
  strictEqual(s.destroyed, true);

  c.close();
  await done.promise;
  ep.close();
}

// Sync throw in onorigin callback destroys the session.
{
  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = (stream) => {
      stream.onheaders = (headers) => {
        stream.sendHeaders({ ':status': '200' });
        stream.writer.endSync();
      };
    };
    await ss.closed;
  }), {
    sni: {
      '*': { keys: [key], certs: [cert] },
      'example.com': { keys: [key], certs: [cert] },
    },
    transportParams: { maxIdleTimeout: 1 },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'example.com',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
    onorigin: mustCall(function() {
      throw new Error('onorigin error');
    }),
    onerror: mustCall(function(error) {
      strictEqual(error.message, 'onorigin error');
    }),
  });
  await clientSession.opened;

  const stream = await clientSession.request({
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'example.com',
  });

  // The session is destroyed by the callback error, which
  // destroys the stream with the same error.
  await rejects(stream.closed, mustCall((err) => {
    strictEqual(err.message, 'onorigin error');
    return true;
  }));

  await rejects(clientSession.closed, mustCall(() => true));

  serverEndpoint.close();
}

// Sync throw in onwanttrailers callback destroys the
// server stream. The server stream's closed promise rejects with
// the thrown error.
{
  const serverStreamRejected = Promise.withResolvers();
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      stream.onheaders = mustCall((headers) => {
        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync(encoder.encode('body'));
        stream.writer.endSync();
      });
      stream.onwanttrailers = mustCall(() => {
        throw new Error('onwanttrailers error');
      });
      // The server stream rejects because onwanttrailers threw.
      await rejects(stream.closed, mustCall((err) => {
        strictEqual(err.message, 'onwanttrailers error');
        serverStreamRejected.resolve();
        return true;
      }));
    });
    await ss.closed;
    serverDone.resolve();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    transportParams: { maxIdleTimeout: 1 },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });
  await clientSession.opened;

  const stream = await clientSession.request({
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
  });

  // Verify the server stream was destroyed by the throw.
  await serverStreamRejected.promise;

  // The client stream is still open (server error doesn't propagate
  // to client automatically). Closing the client session destroys it.
  clientSession.close();
  await Promise.all([stream.closed, serverDone.promise]);
  await serverEndpoint.close();
}
