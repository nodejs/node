'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const tls = require('tls');

const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};

const server = tls.createServer(options, common.mustCall((conn) => {
  setTimeout(() => conn.write('x'), 50);
}));

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
  }, common.mustCall(() => {
    client._parent = undefined;
  }));

  client.on('data', common.mustCall(() => {
    server.close();
    client.destroy();
  }));

  client.on('error', common.mustNotCall());
}));
