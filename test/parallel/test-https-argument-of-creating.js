'use strict';

const common = require('../common');

const assert = require('assert');
const https = require('https');
const tls = require('tls');

const dftProtocol = {};
tls.convertNPNProtocols([ 'http/1.1' ], dftProtocol);

// test for immutable `opts`
const opts = { foo: 'bar', NPNProtocols: [ 'http/1.1' ] };
const server1 = https.createServer(opts);

assert.deepStrictEqual(opts, { foo: 'bar', NPNProtocols: [ 'http/1.1' ] });
assert.strictEqual(server1.NPNProtocols.compare(dftProtocol.NPNProtocols), 0);


// validate that `createServer` can work with the only argument requestListener
const mustNotCall = common.mustNotCall();
const server2 = https.createServer(mustNotCall);

tls.convertNPNProtocols([ 'http/1.1', 'http/1.0' ], dftProtocol);
assert.ok(server2);
assert.strictEqual(server2.NPNProtocols.compare(dftProtocol.NPNProtocols), 0);
assert.strictEqual(server2.listeners('request').length, 1);
assert.strictEqual(server2.listeners('request')[0], mustNotCall);


// validate that `createServer` can work with no arguments
const server3 = https.createServer();

assert.ok(server3);
assert.strictEqual(server3.NPNProtocols.compare(dftProtocol.NPNProtocols), 0);
assert.strictEqual(server3.listeners('request').length, 0);
