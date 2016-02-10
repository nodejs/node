'use strict';
/*
 * This is a regression test for https://github.com/nodejs/node/issues/4060
 */
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
// use -i to force node into interactive mode, despite stdout not being a TTY
const args = ['-i'];
const child = spawn(process.execPath, args);

const input = 'function a() {}; a({}\n)';
// Match '...' as well since it marks a multi-line statement
const expectOut = /^> ... undefined\n/;

child.stderr.on('data', common.fail);

let out = '';
child.stdout.on('data', (c) => {
  out += c;
});

child.stdout.on('end', common.mustCall(() => {
  assert(expectOut.test(out));
}));

child.stdin.end(input);
