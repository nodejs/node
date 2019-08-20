'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

function CheckNoSignalAndErrorCodeOne(code, signal) {
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 1);
}

const child = spawn(process.execPath, [
  '--trace-event-categories', 'madeup', '-e', 'throw new Error()'
], { stdio: [ 'inherit', 'inherit', 'pipe' ] });
child.on('exit', common.mustCall(CheckNoSignalAndErrorCodeOne));

let stderr;
child.stderr.setEncoding('utf8');
child.stderr.on('data', (chunk) => stderr += chunk);
child.stderr.on('end', common.mustCall(() => {
  assert(stderr.includes('throw new Error()'), stderr);
  assert(!stderr.includes('Could not open trace file'), stderr);
}));
