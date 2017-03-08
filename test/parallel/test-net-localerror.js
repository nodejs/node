'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

connect({
  host: 'localhost',
  port: common.PORT,
  localPort: 'foobar',
}, common.expectsError({code: 'ERR_INVALID_PORT', type: RangeError}));

connect({
  host: 'localhost',
  port: common.PORT,
  localAddress: 'foobar',
}, common.expectsError({code: 'ERR_INVALID_IP', type: Error}));

function connect(opts, msg) {
  assert.throws(function() {
    net.connect(opts);
  }, msg);
}
