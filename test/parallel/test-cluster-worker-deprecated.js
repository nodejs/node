'use strict';
require('../common');

const assert = require('assert');
const cluster = require('cluster');

const worker = new cluster.Worker();

assert.strictEqual(worker.exitedAfterDisconnect, undefined);
assert.strictEqual(worker.exitedAfterDisconnect, undefined);

worker.exitedAfterDisconnect = 'recommended';
assert.strictEqual(worker.exitedAfterDisconnect, 'recommended');
assert.strictEqual(worker.exitedAfterDisconnect, 'recommended');

worker.exitedAfterDisconnect = 'deprecated';
assert.strictEqual(worker.exitedAfterDisconnect, 'deprecated');
assert.strictEqual(worker.exitedAfterDisconnect, 'deprecated');
