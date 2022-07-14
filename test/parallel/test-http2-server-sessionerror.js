// Flags: --expose-internals

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');
const { kSocket } = require('internal/http2/util');
const { ServerHttp2Session } = require('internal/http2/core');

const server = http2.createServer();
server.on('stream', common.mustNotCall());

let test = 0;

server.on('session', common.mustCall((session) => {
  assert.strictEqual(session instanceof ServerHttp2Session, true);
  switch (++test) {
    case 1:
      server.on('error', common.mustNotCall());
      session.on('error', common.expectsError({
        name: 'Error',
        message: 'test'
      }));
      session[kSocket].emit('error', new Error('test'));
      break;
    case 2:
      // If the server does not have a socketError listener,
      // error will be silent on the server but will close
      // the session
      session[kSocket].emit('error', new Error('test'));
      break;
  }
}, 2));

server.on('sessionError', common.mustCall((err, session) => {
  assert.strictEqual(err.name, 'Error');
  assert.strictEqual(err.message, 'test');
  assert.strictEqual(session instanceof ServerHttp2Session, true);
}, 2));

server.listen(0, common.mustCall(() => {
  const url = `http://localhost:${server.address().port}`;
  http2.connect(url)
    .on('error', common.expectsError({
      code: 'ERR_HTTP2_SESSION_ERROR',
      message: 'Session closed with error code 2',
    }))
    .on('close', () => {
      server.removeAllListeners('error');
      http2.connect(url)
        .on('error', common.expectsError({
          code: 'ERR_HTTP2_SESSION_ERROR',
          message: 'Session closed with error code 2',
        }))
        .on('close', () => server.close());
    });
}));
