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
        type: Error,
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
    // An ECONNRESET error may occur depending on the platform (due largely
    // to differences in the timing of socket closing). Do not wrap this in
    // a common must call.
    .on('error', () => {})
    .on('close', () => {
      server.removeAllListeners('error');
      http2.connect(url)
        .on('error', () => {})
        .on('close', () => server.close());
    });
}));
