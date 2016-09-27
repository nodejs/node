'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');
const net = require('net');
const assert = require('assert');

const bonkers = Buffer.alloc(1024, 42);

let clientErrorEmited = false;

const server = tls.createServer({})
  .listen(0, function() {
    const c = net.connect({ port: this.address().port }, function() {
      c.write(bonkers);
    });

  }).on('clientError', function(e) {
    clientErrorEmited = true;
    assert.ok(e instanceof Error,
              'Instance of Error should be passed to error handler');
    assert.ok(e.message.match(
      /SSL routines:SSL23_GET_CLIENT_HELLO:unknown protocol/),
      'Expecting SSL unknown protocol');
  });

setTimeout(function() {
  server.close();

  assert.ok(clientErrorEmited, 'clientError should be emited');

}, common.platformTimeout(200));
