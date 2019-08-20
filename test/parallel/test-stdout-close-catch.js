'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const fixtures = require('../common/fixtures');
const { getSystemErrorName } = require('util');

const testScript = fixtures.path('catch-stdout-error.js');

const cmd = `${JSON.stringify(process.execPath)} ` +
            `${JSON.stringify(testScript)} | ` +
            `${JSON.stringify(process.execPath)} ` +
            '-pe "process.stdin.on(\'data\' , () => process.exit(1))"';

const child = child_process.exec(cmd);
let output = '';

child.stderr.on('data', function(c) {
  output += c;
});


child.on('close', common.mustCall(function(code) {
  try {
    output = JSON.parse(output);
  } catch {
    console.error(output);
    process.exit(1);
  }

  assert.strictEqual(output.code, 'EPIPE');
  assert.strictEqual(getSystemErrorName(output.errno), 'EPIPE');
  assert.strictEqual(output.syscall, 'write');
  console.log('ok');
}));
