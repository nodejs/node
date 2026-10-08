// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { createConnection } = require('node:net');
const { internalBinding } = require('internal/test/binding');
const { TCP } = internalBinding('tcp_wrap');
const { UV_ECONNREFUSED } = internalBinding('uv');

// When the last attempt fails while an earlier one is still pending, the
// pending attempt gets one more attempt timeout instead of waiting for the
// operating system. Errors are reported in attempt order.
TCP.prototype.connect = function(req, address, port) {
  if (address === '10.0.0.2') {
    setImmediate(() => req.oncomplete(UV_ECONNREFUSED, this, req, false, false));
  }
  // Attempts to 10.0.0.1 never complete.
  return 0;
};

const connection = createConnection({
  host: 'example.org',
  port: 10,
  lookup: common.mustCall((host, options, callback) => {
    process.nextTick(callback, null, [
      { address: '10.0.0.1', family: 4 },
      { address: '10.0.0.2', family: 4 },
    ]);
  }),
  autoSelectFamily: true,
  autoSelectFamilyAttemptTimeout: 10,
});

connection.on('connectionAttemptTimeout', common.mustCall((address) => {
  assert.strictEqual(address, '10.0.0.1');
}));
connection.on('connectionAttemptFailed', common.mustCall((address) => {
  assert.strictEqual(address, '10.0.0.2');
}));
connection.on('connect', common.mustNotCall());
connection.on('error', common.mustCall((error) => {
  assert.strictEqual(error.constructor.name, 'AggregateError');
  assert.deepStrictEqual(error.errors.map((e) => [e.address, e.code]), [
    ['10.0.0.1', 'ETIMEDOUT'],
    ['10.0.0.2', 'ECONNREFUSED'],
  ]);
}));
connection.on('close', common.mustCall());
