'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const { Agent } = require('_http_agent');

const agent = new Agent({
  keepAlive: true,
  keepAliveMsecs: 1000,
});

const server = http.createServer(common.mustCall((req, res) => {
  res.end('ok');
}));

server.listen(0, common.mustCall(() => {
  const createConnection = agent.createConnection;
  agent.createConnection = (options, ...args) => {
    assert.strictEqual(options.keepAlive, true);
    assert.strictEqual(options.keepAliveInitialDelay, agent.keepAliveMsecs);
    return createConnection.call(agent, options, ...args);
  };
  http.get({
    host: 'localhost',
    port: server.address().port,
    agent: agent,
    path: '/'
  }, common.mustCall((res) => {
    // for emit end event
    res.on('data', () => {});
    res.on('end', () => {
      server.close();
    });
  }));
}));
