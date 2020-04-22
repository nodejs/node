'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const { strictEqual } = require('assert');
const h2 = require('http2');

const server = h2.createServer();
const _error = new Error('some error');

// Check if `sessionError` and `aborted` events
// are emitted if `session` emits an `error` event
server.on('aborted', common.mustCall((error, _ins) => {
  strictEqual(error, _error);
}));

server.on('sessionError', common.mustCall((error, _ins) => {
  strictEqual(error, _error);
}));

server.on('session', common.mustCall((session) => {
  session.destroy(_error);
  server.close();
}));

server.listen(0, common.mustCall(() => {
  const session = h2.connect(`http://localhost:${server.address().port}`);
  session.close();
}));
