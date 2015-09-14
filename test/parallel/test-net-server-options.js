'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

assert.throws(function() { net.createServer('path'); }, TypeError);
assert.throws(function() { net.createServer(common.PORT); }, TypeError);
