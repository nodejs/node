'use strict';

// Regression test: when the server abruptly destroys the underlying socket,
// session.request() must not throw synchronously inside a stream 'close'
// callback. Instead it should return a stream that emits 'error' async with
// ERR_HTTP2_INVALID_SESSION, giving session lifecycle handlers a chance to
// clear their cached session reference before any retry occurs.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('node:http2');

const server = http2.createServer();
let serverSocket;

server.on('connection', (socket) => {
  serverSocket = socket;
  socket.on('error', () => {});
});

server.on('sessionError', () => {});
server.on('stream', (stream, headers) => {
  if (headers[':path'] === '/close') {
    stream.respond({ ':status': 200 });
    stream.write('partial', () => {
      setImmediate(() => serverSocket.destroy());
    });
    return;
  }
  stream.respond({ ':status': 200 });
  stream.end('ok');
});

server.listen(0, common.mustCall(() => {
  const session = http2.connect(`http://localhost:${server.address().port}`);

  session.on('close', common.mustCall(() => server.close()));
  session.on('error', () => {});

  const req = session.request({ ':path': '/close' });
  req.resume();
  req.on('response', () => {});
  req.on('error', () => {}); // socket may emit ECONNRESET on abrupt close

  req.on('close', common.mustCall(() => {
    if (!session.destroyed) return;

    // session.request() must NOT throw synchronously even though the session
    // is already destroyed. It should return a stream that errors async.
    let threw = false;
    let req2;
    try {
      req2 = session.request({ ':path': '/again' });
    } catch {
      threw = true;
    }

    assert.strictEqual(threw, false,
      'session.request() must not throw synchronously inside a stream close callback');

    // The returned stream should asynchronously emit ERR_HTTP2_INVALID_SESSION
    req2.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_HTTP2_INVALID_SESSION');
    }));
    req2.resume();
  }));
}));
