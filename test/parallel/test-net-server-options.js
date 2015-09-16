'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

assert.throws(function() { net.createServer('path'); }, TypeError);
assert.throws(function() { net.createServer(common.PORT); }, TypeError);
