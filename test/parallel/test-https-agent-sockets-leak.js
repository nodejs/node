'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fs = require('fs');
const https = require('https');
const assert = require('assert');

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`),
  ca: fs.readFileSync(`${common.fixturesDir}/keys/ca1-cert.pem`)
};

const server = https.Server(options, common.mustCall((req, res) => {
  res.writeHead(200);
  res.end('https\n');
}));

const agent = new https.Agent({
  keepAlive: false
});

server.listen(0, common.mustCall(() => {
  https.get({
    host: server.address().host,
    port: server.address().port,
    headers: {host: 'agent1'},
    rejectUnauthorized: true,
    ca: options.ca,
    agent: agent
  }, common.mustCall((res) => {
    res.resume();
    server.close();

    // Only one entry should exist in agent.sockets pool
    // If there are more entries in agent.sockets,
    // removeSocket will not delete them resulting in a resource leak
    assert.strictEqual(Object.keys(agent.sockets).length, 1);

    res.req.on('close', common.mustCall(() => {
      // To verify that no leaks occur, check that no entries
      // exist in agent.sockets pool after both request and socket
      // has been closed.
      assert.strictEqual(Object.keys(agent.sockets).length, 0);
    }));
  }));
}));
