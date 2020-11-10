'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const https = require('https');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const connections = {};

const server = https.createServer(options, (req, res) => {
  const interval = setInterval(() => {
    res.write('data');
  }, 1000);
  interval.unref();
});

server.on('connection', (connection) => {
  const key = `${connection.remoteAddress}:${connection.remotePort}`;
  connection.on('close', () => {
    delete connections[key];
  });
  connections[key] = connection;
});

function shutdown() {
  server.close(common.mustCall());

  for (const key in connections) {
    connections[key].destroy();
    delete connections[key];
  }
}

server.listen(0, () => {
  const requestOptions = {
    hostname: '127.0.0.1',
    port: server.address().port,
    path: '/',
    method: 'GET',
    rejectUnauthorized: false
  };

  const req = https.request(requestOptions, (res) => {
    res.on('data', () => {});
    setImmediate(shutdown);
  });
  req.end();
});
