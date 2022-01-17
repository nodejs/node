'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const { once } = require('events');
const Agent = https.Agent;
const fixtures = require('../common/fixtures');

const { getEventListeners } = require('events');
const agent = new Agent();

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = https.createServer(options);

server.listen(0, common.mustCall(async () => {
  const port = server.address().port;
  const host = 'localhost';
  const options = {
    port: port,
    host: host,
    rejectUnauthorized: false,
    _agentKey: agent.getName({ port, host })
  };

  async function postCreateConnection() {
    const ac = new AbortController();
    const { signal } = ac;
    const connection = agent.createConnection({ ...options, signal });
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    ac.abort();
    const [err] = await once(connection, 'error');
    assert.strictEqual(err.name, 'AbortError');
  }

  async function preCreateConnection() {
    const ac = new AbortController();
    const { signal } = ac;
    ac.abort();
    const connection = agent.createConnection({ ...options, signal });
    const [err] = await once(connection, 'error');
    assert.strictEqual(err.name, 'AbortError');
  }


  async function agentAsParam() {
    const ac = new AbortController();
    const { signal } = ac;
    const request = https.get({
      port: server.address().port,
      path: '/hello',
      agent: agent,
      signal,
    });
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    ac.abort();
    const [err] = await once(request, 'error');
    assert.strictEqual(err.name, 'AbortError');
  }

  async function agentAsParamPreAbort() {
    const ac = new AbortController();
    const { signal } = ac;
    ac.abort();
    const request = https.get({
      port: server.address().port,
      path: '/hello',
      agent: agent,
      signal,
    });
    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
    const [err] = await once(request, 'error');
    assert.strictEqual(err.name, 'AbortError');
  }

  await postCreateConnection();
  await preCreateConnection();
  await agentAsParam();
  await agentAsParamPreAbort();
  server.close();
}));
