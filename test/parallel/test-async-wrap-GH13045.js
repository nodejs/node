'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Refs: https://github.com/nodejs/node/issues/13045
// An HTTP Agent reuses a TLSSocket, and makes a failed call to `asyncReset`.

const assert = require('assert');
const https = require('https');
const fs = require('fs');

const serverOptions = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`),
  ca: fs.readFileSync(`${common.fixturesDir}/keys/ca1-cert.pem`)
};

const server = https.createServer(serverOptions, common.mustCall((req, res) => {
  res.end('hello world\n');
}, 2));

server.listen(0, common.mustCall(function() {
  const port = this.address().port;
  const clientOptions = {
    agent: new https.Agent({
      keepAlive: true,
      rejectUnauthorized: false
    }),
    port: port
  };

  const req = https.get(clientOptions, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    res.on('error', (err) => assert.fail(err));
    res.socket.on('error', (err) => assert.fail(err));
    res.resume();
    // drain the socket and wait for it to be free to reuse
    res.socket.once('free', () => {
      // This is the pain point. Internally the Agent will call
      // `socket._handle.asyncReset()` and if the _handle does not implement
      // `asyncReset` this will throw TypeError
      const req2 = https.get(clientOptions, common.mustCall((res2) => {
        assert.strictEqual(res.statusCode, 200);
        res2.on('error', (err) => assert.fail(err));
        res2.socket.on('error', (err) => assert.fail(err));
        // this should be the end of the test
        res2.destroy();
        server.close();
      }));
      req2.on('error', (err) => assert.fail(err));
    });
  }));
  req.on('error', (err) => assert.fail(err));
}));
