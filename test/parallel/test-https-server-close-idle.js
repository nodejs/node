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

let connections = 0;

const server = createServer(options, common.mustCall(function(req, res) {
  res.writeHead(200, { Connection: 'keep-alive' });
  res.end();
}), {
  headersTimeout: 0,
  keepAliveTimeout: 0,
  requestTimeout: common.platformTimeout(60000),
});

server.on('connection', function() {
  connections++;
});

server.listen(0, function() {
  const port = server.address().port;
  let client1Closed = false;
  let client2Closed = false;

  // Create a first request but never finish it
  const client1 = connect({ port, rejectUnauthorized: false });

  client1.on('close', common.mustCall(() => {
    client1Closed = true;
  }));

  client1.on('error', () => {});

  client1.write('GET / HTTP/1.1');

  // Create a second request, let it finish but leave the connection opened using HTTP keep-alive
  const client2 = connect({ port, rejectUnauthorized: false });
  let response = '';

  client2.on('data', common.mustCall((chunk) => {
    response += chunk.toString('utf-8');

    if (response.endsWith('0\r\n\r\n')) {
      assert(response.startsWith('HTTP/1.1 200 OK\r\nConnection: keep-alive'));
      assert.strictEqual(connections, 2);

      server.closeIdleConnections();
      server.close(common.mustCall());

      // Check that only the idle connection got closed
      setTimeout(common.mustCall(() => {
        assert(!client1Closed);
        assert(client2Closed);

        server.closeAllConnections();
        server.close(common.mustCall());
      }), common.platformTimeout(500)).unref();
    }
  }));

  client2.on('close', common.mustCall(() => {
    client2Closed = true;
  }));

  client2.write('GET / HTTP/1.1\r\n\r\n');
});
