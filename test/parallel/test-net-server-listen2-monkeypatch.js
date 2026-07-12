'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// Overriding the deprecated `Server.prototype._listen2` alias (DEP0208) is
// still honored by `server.listen()`.

common.expectWarning(
  'DeprecationWarning',
  'Server.prototype._listen2 is deprecated. Use Server.prototype.listen() instead.',
  'DEP0208');

const original = net.Server.prototype._listen2;
net.Server.prototype._listen2 = common.mustCall(function(...args) {
  assert.strictEqual(this, server);
  assert.deepStrictEqual(args, [null, 0, 4, 0, undefined, 0]);
  return original.apply(this, args);
});

const server = net.createServer();
server.listen(0, common.mustCall(() => {
  net.Server.prototype._listen2 = original;
  server.close();
}));
