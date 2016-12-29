// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const errors = require('internal/errors');

const invalidPort = -1 >>> 0;
const errorMessage = new RegExp(`${errors.message('INVALID_PORT')}`);

net.Server().listen(common.PORT, function() {
  const address = this.address();
  const key = `${address.family.slice(-1)}:${address.address}:${common.PORT}`;

  assert.equal(this._connectionKey, key);
  this.close();
});

// The first argument is a configuration object
assert.throws(() => {
  net.Server().listen({ port: invalidPort }, common.fail);
}, errorMessage);

// The first argument is the port, no IP given.
assert.throws(() => {
  net.Server().listen(invalidPort, common.fail);
}, errorMessage);

// The first argument is the port, the second an IP.
assert.throws(() => {
  net.Server().listen(invalidPort, '0.0.0.0', common.fail);
}, errorMessage);
