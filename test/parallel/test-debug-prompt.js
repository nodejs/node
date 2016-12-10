'use strict';

const assert = require('assert');
const common = require('../common');
const spawn = require('child_process').spawn;

const proc = spawn(process.execPath, ['debug', 'foo']);
proc.stdout.setEncoding('utf8');

proc.stdout.once('data', common.mustCall((data) => {
  assert.strictEqual(data, 'debug> ');
  proc.kill();
}));
