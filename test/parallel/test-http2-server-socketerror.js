// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.end('ok');
}));
server.on('session', common.mustCall((session) => {
  // First, test that the socketError event is forwarded to the session object
  // and not the server object.
  const handler = common.mustCall((error, socket) => {
    common.expectsError({
      type: Error,
      message: 'test'
    })(error);
    assert.strictEqual(socket, session.socket);
  });
  const isNotCalled = common.mustNotCall();
  session.on('socketError', handler);
  server.on('socketError', isNotCalled);
  session.socket.emit('error', new Error('test'));
  session.removeListener('socketError', handler);
  server.removeListener('socketError', isNotCalled);

  // Second, test that the socketError is forwarded to the server object when
  // no socketError listener is registered for the session
  server.on('socketError', common.mustCall((error, socket, session) => {
    common.expectsError({
      type: Error,
      message: 'test'
    })(error);
    assert.strictEqual(socket, session.socket);
    assert.strictEqual(session, session);
  }));
  session.socket.emit('error', new Error('test'));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.resume();
  req.on('end', common.mustCall());
  req.on('streamClosed', common.mustCall(() => {
    client.destroy();
    server.close();
  }));
}));
