'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

const child = spawn(process.execPath, ['-e', `
    const server = require('net').createServer().listen(0);
    server.once('listening', server.close);
`]);

child.once('exit', common.mustCall(function(exitCode, signalCode) {
  assert.strictEqual(exitCode, 0);
  assert.strictEqual(signalCode, null);
}));
