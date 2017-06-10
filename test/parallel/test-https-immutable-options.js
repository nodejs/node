'use strict';

const common = require('../common');

const assert = require('assert');
const https = require('https');
const tls = require('tls');

const dftProtocol = {};
tls.convertNPNProtocols([ 'http/1.1', 'http/1.0' ], dftProtocol);

const opts = { foo: 'bar' };
const server1 = https.createServer(opts);

assert.deepStrictEqual(opts, { foo: 'bar' });
assert.strictEqual(server1.NPNProtocols.compare(dftProtocol.NPNProtocols), 0);

const mustNotCall = common.mustNotCall();
const server2 = https.createServer(mustNotCall);

assert.strictEqual(server2.NPNProtocols.compare(dftProtocol.NPNProtocols), 0);
assert.strictEqual(mustNotCall, server2._events.request);
