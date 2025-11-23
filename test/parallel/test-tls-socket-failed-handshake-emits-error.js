'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const net = require('net');
const assert = require('assert');

const bonkers = Buffer.alloc(1024, 42);

const server = net.createServer(common.mustCall((c) => {
  setTimeout(common.mustCall(() => {
    const s = new tls.TLSSocket(c, {
      isServer: true,
      server: server
    });

    s.on('error', common.mustCall(function(e) {
      assert.ok(e instanceof Error,
                'Instance of Error should be passed to error handler');
      assert.match(
        e.message,
        /SSL routines:[^:]*:wrong version number/,
      );
    }));

    s.on('close', function() {
      server.close();
      s.destroy();
    });
  }), common.platformTimeout(200));
})).listen(0, function() {
  const c = net.connect({ port: this.address().port }, function() {
    c.write(bonkers);
  });
});
