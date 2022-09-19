'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

// Check that, once a child process has ended, it’s safe to read from a pipe
// that the child had used as input.
// We simulate that using cat | (head -n1; ...)

const p1 = spawn('cat', { stdio: ['pipe', 'pipe', 'inherit'] });
const p2 = spawn('head', ['-n1'], { stdio: [p1.stdout, 'pipe', 'inherit'] });

// First, write the line that gets passed through p2, making 'head' exit.
p1.stdin.write('hello\n');
p2.stdout.setEncoding('utf8');
p2.stdout.on('data', common.mustCall((chunk) => {
  assert.strictEqual(chunk, 'hello\n');
}));
p2.on('exit', common.mustCall(() => {
  // We can now use cat’s output, because 'head' is no longer reading from it.
  p1.stdin.end('world\n');
  p1.stdout.setEncoding('utf8');
  p1.stdout.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk, 'world\n');
  }));
  p1.stdout.resume();
}));
