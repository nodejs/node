'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const util = require('util');
const fixtures = require('../common/fixtures');

const sent = 'hello world';
const serverOptions = {
  isServer: true,
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

let ssl = null;

process.on('exit', function() {
  assert.ok(ssl !== null);
  // If the internal pointer to stream_ isn't cleared properly then this
  // will abort.
  util.inspect(ssl);
});

const server = tls.createServer(serverOptions, function(s) {
  s.on('data', function() { });
  s.on('end', function() {
    server.close();
    s.destroy();
  });
}).listen(0, function() {
  const c = new tls.TLSSocket();
  ssl = c.ssl;
  c.connect(this.address().port, function() {
    c.end(sent);
  });
});
