'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const tls = require('tls');
const fs = require('fs');
const net = require('net');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
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
