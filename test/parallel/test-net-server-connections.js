'use strict';
require('../common');
const assert = require('assert');

const net = require('net');

// test that server.connections property is no longer enumerable now that it
// has been marked as deprecated

const server = new net.Server();

assert.equal(Object.keys(server).indexOf('connections'), -1);
