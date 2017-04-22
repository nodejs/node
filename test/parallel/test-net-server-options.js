'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

assert.throws(function() { net.createServer('path'); },
              /^TypeError: options must be an object$/);
assert.throws(function() { net.createServer(0); },
              /^TypeError: options must be an object$/);
