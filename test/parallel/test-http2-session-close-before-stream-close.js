'use strict';

// Regression test: when the server abruptly destroys the underlying socket,
// the client session 'close' event must fire before any stream 'close'
// callback can observe session.closed/session.destroyed as true.
// See https://github.com/nodejs/node/issues/<issue>

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

  let sessionCloseFired = false;

  session.on('close', common.mustCall(() => {
    sessionCloseFired = true;
    server.close();
  }));

  // We accept that an error may or may not fire, but it must fire before
  // any stream close callback sees session.destroyed.
  session.on('error', () => {});

  const req = session.request({ ':path': '/close' });
  req.resume();
  req.on('response', () => {});

  // The stream 'close' event must not observe the session as destroyed
  // before the session 'close' event has fired.
  req.on('close', common.mustCall(() => {
    // If the session is already destroyed, the session 'close' event must
    // have already been emitted (sessionCloseFired === true).
    if (session.destroyed) {
      assert.strictEqual(
        sessionCloseFired,
        true,
        'session "close" event must fire before stream "close" callback ' +
        'observes session.destroyed === true',
      );
    }
  }));
}));
