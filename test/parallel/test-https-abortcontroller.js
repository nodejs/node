'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const https = require('https');
const assert = require('assert');
const { once, getEventListeners } = require('events');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

// Check async post-aborted
(async () => {
  const { port, server } = await new Promise((resolve) => {
    const server = https.createServer(options, () => {});
    server.listen(0, () => resolve({ port: server.address().port, server }));
  });
  try {
    const ac = new AbortController();
    const req = https.request({
      host: 'localhost',
      port,
      path: '/',
      method: 'GET',
      rejectUnauthorized: false,
      signal: ac.signal,
    });
    assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 1);
    process.nextTick(() => ac.abort());
    const [ err ] = await once(req, 'error');
    assert.strictEqual(err.name, 'AbortError');
    assert.strictEqual(err.code, 'ABORT_ERR');
  } finally {
    server.close();
  }
})().then(common.mustCall());

// Check sync post-aborted signal
(async () => {
  const { port, server } = await new Promise((resolve) => {
    const server = https.createServer(options, () => {});
    server.listen(0, () => resolve({ port: server.address().port, server }));
  });
  try {
    const ac = new AbortController();
    const { signal } = ac;
    const req = https.request({
      host: 'localhost',
      port,
      path: '/',
      method: 'GET',
      rejectUnauthorized: false,
      signal,
    });
    assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 1);
    ac.abort();
    const [ err ] = await once(req, 'error');
    assert.strictEqual(err.name, 'AbortError');
    assert.strictEqual(err.code, 'ABORT_ERR');
  } finally {
    server.close();
  }
})().then(common.mustCall());

// Check pre-aborted signal
(async () => {
  const { port, server } = await new Promise((resolve) => {
    const server = https.createServer(options, () => {});
    server.listen(0, () => resolve({ port: server.address().port, server }));
  });
  try {
    const ac = new AbortController();
    const { signal } = ac;
    ac.abort();
    const req = https.request({
      host: 'localhost',
      port,
      path: '/',
      method: 'GET',
      rejectUnauthorized: false,
      signal,
    });
    assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);
    const [ err ] = await once(req, 'error');
    assert.strictEqual(err.name, 'AbortError');
    assert.strictEqual(err.code, 'ABORT_ERR');
  } finally {
    server.close();
  }
})().then(common.mustCall());
