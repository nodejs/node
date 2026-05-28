'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
let serverSocket;

server.on('connection', common.mustCall((socket) => {
  serverSocket = socket;
  socket.on('error', () => {});
}));

server.on('sessionError', () => {});
server.on('stream', common.mustCall((stream, headers) => {
  if (headers[':path'] === '/close') {
    stream.respond({ ':status': 200 });
    stream.write('partial', common.mustCall(() => {
      setImmediate(() => serverSocket.destroy());
    }));
    return;
  }

  stream.respond({ ':status': 200 });
  stream.end('ok');
}));

server.listen(0, common.mustCall(() => {
  const session = http2.connect(`http://localhost:${server.address().port}`);
  let cachedSession = session;

  session.on('error', () => {});
  session.on('close', common.mustCall(() => {
    cachedSession = undefined;
    server.close();
  }));

  const req = session.request({ ':path': '/close' });
  req.on('response', common.mustCall());
  req.on('error', () => {});
  req.on('close', common.mustCall(() => {
    // This must not throw synchronously even though the session is no longer
    // usable. Depending on teardown timing, the returned stream may report a
    // closed session before the destroy state is fully observable here.
    const req2 = session.request({ ':path': '/again' });

    req2.on('error', common.mustCall((err) => {
      assert.ok(
        err.code === 'ERR_HTTP2_INVALID_SESSION' ||
        err.code === 'ERR_HTTP2_GOAWAY_SESSION');
      assert.strictEqual(cachedSession, undefined);
    }));
  }));
  req.resume();
}));
