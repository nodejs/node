'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

// Using port 0 as localPort / localAddress is already invalid.
connect({
  host: 'localhost',
  port: 0,
  localPort: 'foobar',
}, /^TypeError: "localPort" option should be a number: foobar$/);

connect({
  host: 'localhost',
  port: 0,
  localAddress: 'foobar',
}, /^TypeError: "localAddress" option must be a valid IP: foobar$/);

function connect(opts, msg) {
  assert.throws(() => {
    net.connect(opts);
  }, msg);
}
