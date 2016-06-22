'use strict';
// Refs: https://github.com/nodejs/node/issues/7342
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

const keepAlive = setInterval(() => {}, 9999);

const command = common.isWindows ? 'dir' : 'ls';
const e = exec(command);

e.stdout.on('data', function(data) {
  assert.strictEqual(typeof data, 'string');
  clearInterval(keepAlive);
});
