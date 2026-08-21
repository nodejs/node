'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

const child = cp.spawn(process.execPath, ['--interactive']);

let output = '';
child.stdout.on('data', (data) => {
  output += data;
});

child.on('exit', common.mustCall(() => {
  assert.doesNotMatch(output, /Uncaught Error/);
}));

child.stdin.write('await null;\n');
child.stdin.write('.exit\n');
