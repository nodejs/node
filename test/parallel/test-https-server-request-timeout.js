'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const { createServer } = require('https');
const { connect } = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

// This test validates that the server returns 408
// after server.requestTimeout if the client
// pauses before start sending the body.

const server = createServer(options, common.mustCall((req, res) => {
  let body = '';
  req.setEncoding('utf-8');

  req.on('data', (chunk) => {
    body += chunk;
  });

  req.on('end', () => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.write(body);
    res.end();
  });
}));

// 0 seconds is the default
assert.strictEqual(server.requestTimeout, 0);
const requestTimeout = common.platformTimeout(1000);
server.requestTimeout = requestTimeout;
assert.strictEqual(server.requestTimeout, requestTimeout);

server.listen(0, common.mustCall(() => {
  const client = connect({
    host: '127.0.0.1',
    port: server.address().port,
    rejectUnauthorized: false
  });
  let response = '';

  client.on('data', common.mustCall((chunk) => {
    response += chunk.toString('utf-8');
  }));

  client.resume();
  client.write('POST / HTTP/1.1\r\n');
  client.write('Content-Length: 20\r\n');
  client.write('Connection: close\r\n');
  client.write('\r\n');

  setTimeout(() => {
    client.write('12345678901234567890\r\n\r\n');
  }, common.platformTimeout(2000)).unref();

  const errOrEnd = common.mustCall(function(err) {
    assert.strictEqual(
      response,
      'HTTP/1.1 408 Request Timeout\r\nConnection: close\r\n\r\n'
    );
    server.close();
  });

  client.on('end', errOrEnd);
  client.on('error', errOrEnd);
}));
