'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { createServer } = require('https');
const { connect } = require('tls');

const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = createServer(options, common.mustCall(function(req, res) {
  res.writeHead(200, { 'Connection': 'keep-alive' });
  res.end();
}), { requestTimeout: common.platformTimeout(60000), headersTimeout: 0 });

server.listen(0, function() {
  const port = server.address().port;

  // Create a new request, let it finish but leave the connection opened using HTTP keep-alive
  const client1 = connect({ port, rejectUnauthorized: false });
  let response1 = '';

  client1.on('data', common.mustCall((chunk) => {
    response1 += chunk.toString('utf-8');
  }));

  client1.on('end', common.mustCall(function() {
    assert(response1.startsWith('HTTP/1.1 200 OK\r\nConnection: keep-alive'));
  }));
  client1.on('close', common.mustCall());

  client1.resume();
  client1.write('GET / HTTP/1.1\r\nConnection: keep-alive\r\n\r\n');

  // Create a second request but never finish it
  const client2 = connect({ port, rejectUnauthorized: false });
  client2.on('close', common.mustCall());

  client2.resume();
  client2.write('GET / HTTP/1.1\r\n');

  // Now, after a while, close the server
  setTimeout(common.mustCall(() => {
    server.closeAllConnections();
    server.close(common.mustCall());
  }), common.platformTimeout(1000));

  // This timer should never go off as the server.close should shut everything down
  setTimeout(common.mustNotCall(), common.platformTimeout(1500)).unref();
});
