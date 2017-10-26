'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();

const spawn = require('child_process').spawn;

const script = `
const assert = require('assert');
const async_wrap = process.binding('async_wrap');
const { kTotals } = async_wrap.constants;

assert.strictEqual(async_wrap.async_hook_fields[kTotals], 4);
process._debugEnd();
assert.strictEqual(async_wrap.async_hook_fields[kTotals], 0);
`;

const args = ['--inspect', '-e', script];
const child = spawn(process.execPath, args, { stdio: 'inherit' });
child.on('exit', (code, signal) => {
  process.exit(code || signal);
});
