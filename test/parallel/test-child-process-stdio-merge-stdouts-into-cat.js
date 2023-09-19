'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

// Regression test for https://github.com/nodejs/node/issues/27097.
// Check that (cat [p1] ; cat [p2]) | cat [p3] works.

const p3 = spawn('cat', { stdio: ['pipe', 'pipe', 'inherit'] });
const p1 = spawn('cat', { stdio: ['pipe', p3.stdin, 'inherit'] });
const p2 = spawn('cat', { stdio: ['pipe', p3.stdin, 'inherit'] });
p3.stdout.setEncoding('utf8');

// Write three different chunks:
// - 'hello' from this process to p1 to p3 back to us
// - 'world' from this process to p2 to p3 back to us
// - 'foobar' from this process  to p3 back to us
// Do so sequentially in order to avoid race conditions.
p1.stdin.end('hello\n');
p3.stdout.once('data', common.mustCall((chunk) => {
  assert.strictEqual(chunk, 'hello\n');
  p2.stdin.end('world\n');
  p3.stdout.once('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk, 'world\n');
    p3.stdin.end('foobar\n');
    p3.stdout.once('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk, 'foobar\n');
    }));
  }));
}));
