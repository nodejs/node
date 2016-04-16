'use strict';
require('../common');
var assert = require('assert');

var net = require('net');

// test that server.connections property is no longer enumerable now that it
// has been marked as deprecated

var server = new net.Server();

assert.equal(Object.keys(server).indexOf('connections'), -1);
