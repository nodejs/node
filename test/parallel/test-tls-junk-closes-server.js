'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const net = require('net');

const options = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem')
};

const server = tls.createServer(options, common.mustNotCall());

server.listen(0, common.mustCall(function() {
  const c = net.createConnection(this.address().port);

  c.on('connect', common.mustCall(function() {
    c.write('blah\nblah\nblah\n');
  }));

  c.on('end', common.mustCall(function() {
    server.close();
  }));
}));
