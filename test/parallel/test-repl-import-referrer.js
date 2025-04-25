'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fixtures = require('../common/fixtures');

const args = ['--interactive'];
const opts = { cwd: fixtures.path('es-modules') };
const child = cp.spawn(process.execPath, args, opts);

let output = '';
child.stdout.setEncoding('utf8');
child.stdout.on('data', (data) => {
  output += data;

  if (
    !child.stdin.writableEnded &&
    output.includes('[Module: null prototype] { message: \'A message\' }\n> ')
  ) {
    // All the expected outputs have been received
    // so we can close the child process's stdin
    child.stdin.end();
  }
});

child.on('exit', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}));

child.stdin.write('await import(\'./message.mjs\');\n');
child.stdin.write('.exit');
