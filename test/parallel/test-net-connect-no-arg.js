'use strict';

require('../common');
const assert = require('assert');
const net = require('net');

// Tests that net.connect() called without arguments throws ERR_MISSING_ARGS.

assert.throws(() => {
  net.connect();
}, {
  code: 'ERR_MISSING_ARGS',
  message: 'The "options" or "port" or "path" argument must be specified',
});

assert.throws(() => {
  new net.Socket().connect();
}, {
  code: 'ERR_MISSING_ARGS',
  message: 'The "options" or "port" or "path" argument must be specified',
});

assert.throws(() => {
  net.connect({});
}, {
  code: 'ERR_MISSING_ARGS',
  message: 'The "options" or "port" or "path" argument must be specified',
});

assert.throws(() => {
  new net.Socket().connect({});
}, {
  code: 'ERR_MISSING_ARGS',
  message: 'The "options" or "port" or "path" argument must be specified',
});
