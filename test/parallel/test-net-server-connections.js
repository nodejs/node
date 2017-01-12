'use strict';
const common = require('../common');
const assert = require('assert');

const net = require('net');

const server = new net.Server();

const expectedWarning = 'Server.connections property is deprecated. ' +
                        'Use Server.getConnections method instead.';

common.expectWarning('DeprecationWarning', expectedWarning);

// test that server.connections property is no longer enumerable now that it
// has been marked as deprecated
assert.strictEqual(Object.keys(server).indexOf('connections'), -1);

assert.strictEqual(server.connections, 0);
