'use strict';

// This is a regression test for https://github.com/joyent/node/issues/8874.

require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;
// Use -i to force node into interactive mode, despite stdout not being a TTY
const args = [ '-i' ];
const child = spawn(process.execPath, args);

const input = 'const foo = "bar\\\nbaz"';
// Match '...' as well since it marks a multi-line statement
const expectOut = /> \| undefined\n/;

child.stderr.setEncoding('utf8');
child.stderr.on('data', (d) => {
  throw new Error('child.stderr be silent');
});

child.stdout.setEncoding('utf8');
let out = '';
child.stdout.on('data', (d) => {
  out += d;
});

child.stdout.on('end', () => {
  assert.match(out, expectOut);
  console.log('ok');
});

child.stdin.end(input);
