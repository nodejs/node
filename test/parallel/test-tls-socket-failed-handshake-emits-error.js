'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const net = require('net');
const assert = require('assert');

const bonkers = Buffer.alloc(1024, 42);

const server = net.createServer(function(c) {
  setTimeout(function() {
    const s = new tls.TLSSocket(c, {
      isServer: true,
      server: server
    });

    s.on('error', common.mustCall(function(e) {
      assert.ok(e instanceof Error,
                'Instance of Error should be passed to error handler');
      assert.ok(
        /SSL routines:[^:]*:wrong version number/.test(
          e.message),
        'Expecting SSL unknown protocol');
    }));

    s.on('close', function() {
      server.close();
      s.destroy();
    });
  }, common.platformTimeout(200));
}).listen(0, function() {
  const c = net.connect({ port: this.address().port }, function() {
    c.write(bonkers);
  });
});
