'use strict';
const common = require('../common');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const connections = {};

const server = https.createServer(options, function(req, res) {
  const interval = setInterval(function() {
    res.write('data');
  }, 1000);
  interval.unref();
});

server.on('connection', function(connection) {
  const key = connection.remoteAddress + ':' + connection.remotePort;
  connection.on('close', function() {
    delete connections[key];
  });
  connections[key] = connection;
});

function shutdown() {
  server.close(common.mustCall(function() {}));

  for (const key in connections) {
    connections[key].destroy();
    delete connections[key];
  }
}

server.listen(0, function() {
  const requestOptions = {
    hostname: '127.0.0.1',
    port: this.address().port,
    path: '/',
    method: 'GET',
    rejectUnauthorized: false
  };

  const req = https.request(requestOptions, function(res) {
    res.on('data', function(d) {});
    setImmediate(shutdown);
  });
  req.end();
});
