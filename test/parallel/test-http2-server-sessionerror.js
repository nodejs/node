// Flags: --expose-internals

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const { kSocket } = require('internal/http2/util');

const server = http2.createServer();
server.on('stream', common.mustNotCall());

let test = 0;

server.on('session', common.mustCall((session) => {
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
