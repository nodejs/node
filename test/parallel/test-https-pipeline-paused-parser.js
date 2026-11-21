'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Regression test for https://github.com/nodejs/node/issues/67455
// When a pipelined request arrives while the server is draining a large
// response, the server pauses the socket and the parser. TLSWrap must not
// emit records it has already decrypted after ReadStop(), and must deliver
// them after ReadStart(), or the paused parser fails with HPE_PAUSED.

const assert = require('assert');
const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');

// A small body arrives in one network read, so no later read would deliver
// the records held back during the pause. A large body spans several reads.
for (const bodySize of [20000, 100000]) {
  const server = https.createServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
  }, common.mustCall((req, res) => {
    if (req.method === 'GET') {
      // Large enough that the socket must drain, so the server pauses.
      res.end(Buffer.alloc(1e7));
      return;
    }
    let received = 0;
    req.on('data', (chunk) => received += chunk.length);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(received, bodySize);
      res.end();
      client.destroy();
      server.close();
    }));
  }, 2));

  server.on('clientError', common.mustNotCall());

  let client;
  server.listen(0, common.mustCall(() => {
    client = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
    });
    client.write('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
    // Pipeline a POST while the GET response is still being sent.
    client.once('data', common.mustCall(() => {
      client.write('POST / HTTP/1.1\r\nHost: localhost\r\n' +
                   `Content-Length: ${bodySize}\r\n\r\n` +
                   'x'.repeat(bodySize));
    }));
  }));
}
