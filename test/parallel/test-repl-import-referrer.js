'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fixtures = require('../common/fixtures');

const args = ['--interactive'];
const opts = { cwd: fixtures.path('es-modules') };
const child = cp.spawn(process.execPath, args, opts);

const outputs = [];
child.stdout.setEncoding('utf8');
child.stdout.on('data', (data) => {
  outputs.push(data);
  if (outputs.length === 3) {
    // All the expected outputs have been received
    // so we can close the child process's stdin
    child.stdin.end();
  }
});

child.on('exit', common.mustCall(() => {
  const results = outputs[2].split('\n')[0];
  assert.strictEqual(
    results,
    '[Module: null prototype] { message: \'A message\' }'
  );
}));

// Note: write the commands on stdin with a slight delay to make sure
//       that the child process is ready to receive and process them
setTimeout(() => {
  child.stdin.write('await import(\'./message.mjs\');\n');
  child.stdin.write('.exit');
}, common.platformTimeout(250));
