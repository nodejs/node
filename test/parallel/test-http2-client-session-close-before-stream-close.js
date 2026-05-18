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
  const events = [];

  session.on('error', common.mustNotCall());
  session.on('close', common.mustCall(() => {
    events.push('session-close');
    cachedSession = undefined;
    server.close();
  }));

  const req = session.request({ ':path': '/close' });
  req.on('response', common.mustCall());
  req.on('close', common.mustCall(() => {
    events.push('stream-close');
    assert.strictEqual(session.closed, true);
    assert.strictEqual(session.destroyed, true);
    assert.strictEqual(cachedSession, undefined);
    assert.deepStrictEqual(events, ['session-close', 'stream-close']);
  }));
  req.resume();
}));
