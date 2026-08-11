'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// `Server.prototype._listen2` is a deprecated alias for the internal function
// that sets up the listening handle (DEP0208).

common.expectWarning(
  'DeprecationWarning',
  'Server.prototype._listen2 is deprecated. Use Server.prototype.listen() instead.',
  'DEP0208');

// Listening without touching `_listen2` must not emit the warning.
const server = net.createServer();
server.listen(0, common.mustCall(() => {
  server.close(common.mustCall(() => {
    // Calling the alias directly emits the warning. It still sets up the
    // handle, so the server ends up listening.
    const legacy = net.createServer();
    legacy.on('listening', common.mustCall(() => {
      assert.strictEqual(legacy.listening, true);
      legacy.close();
    }));
    legacy._listen2(null, 0, 4, undefined, undefined, 0);
  }));
}));
