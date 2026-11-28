'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer, get } = require('http');

function listen(server) {
  return new Promise((resolve) => server.listen(0, resolve));
}

// Connections that are still active when the timeout expires are closed.
async function closesActiveConnections() {
  const server = createServer(common.mustCall((req, res) => {
    // Never send a response.
    res.on('close', common.mustCall());
    const ret = server.close({ timeout: common.platformTimeout(100) },
                             common.mustSucceed());
    assert.strictEqual(ret, server);
  }));

  await listen(server);

  const { promise, resolve } = Promise.withResolvers();
  server.on('close', resolve);

  get({ port: server.address().port }, common.mustNotCall())
    .on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECONNRESET');
    }));

  await promise;
}

// A request that completes before the timeout expires is not interrupted,
// and the pending timer does not keep the process alive.
async function doesNotInterruptRequests() {
  const server = createServer(common.mustCall((req, res) => {
    server.close({ timeout: common.platformTimeout(60_000) },
                 common.mustSucceed());
    setTimeout(() => res.end('ok'), common.platformTimeout(50));
  }));

  await listen(server);

  const { promise, resolve } = Promise.withResolvers();
  server.on('close', resolve);

  get({
    port: server.address().port,
    headers: { connection: 'close' },
  }, common.mustCall((res) => {
    let body = '';
    res.setEncoding('utf8');
    res.on('data', (chunk) => { body += chunk; });
    res.on('end', common.mustCall(() => {
      assert.strictEqual(body, 'ok');
    }));
  }));

  await promise;
}

// The options are validated before the server stops listening.
async function validatesOptions() {
  const server = createServer(common.mustNotCall());

  await listen(server);

  for (const timeout of [0, -1, 1.5, 2 ** 31, NaN, Infinity]) {
    assert.throws(() => server.close({ timeout }), {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
    });
  }

  for (const timeout of ['1', null, true, {}, 1n]) {
    assert.throws(() => server.close({ timeout }), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
  }

  assert.strictEqual(server.listening, true);

  // A first argument that is neither an object nor a function is ignored.
  const { promise, resolve } = Promise.withResolvers();
  server.on('close', resolve);
  server.close('SIGTERM');

  await promise;
}

(async () => {
  await closesActiveConnections();
  await doesNotInterruptRequests();
  await validatesOptions();
})().then(common.mustCall());
