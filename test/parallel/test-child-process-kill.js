// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const cat = spawn(common.isWindows ? 'cmd' : 'cat');

cat.stdout.on('end', common.mustCall());
cat.stderr.on('data', common.mustNotCall());
cat.stderr.on('end', common.mustCall());

cat.on('exit', common.mustCall((code, signal) => {
  assert.strictEqual(code, null);
  assert.strictEqual(signal, 'SIGTERM');
  assert.strictEqual(cat.signalCode, 'SIGTERM');
}));

assert.strictEqual(cat.signalCode, null);
assert.strictEqual(cat.killed, false);
cat.kill();
assert.strictEqual(cat.killed, true);

// Test different types of kill signals on Windows.
if (common.isWindows) {
  for (const sendSignal of ['SIGTERM', 'SIGKILL', 'SIGQUIT', 'SIGINT']) {
    const process = spawn('cmd');
    process.on('exit', (code, signal) => {
      assert.strictEqual(code, null);
      assert.strictEqual(signal, sendSignal);
    });
    process.kill(sendSignal);
  }

  const process = spawn('cmd');
  process.on('exit', (code, signal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(signal, 'SIGKILL');
  });
  process.kill('SIGHUP');
}

// Test that the process is not killed when sending a 0 signal.
// This is a no-op signal that is used to check if the process is alive.
const code = `const interval = setInterval(() => {}, 1000);
process.stdin.on('data', () => { clearInterval(interval); });
process.stdout.write('x');`;

const checkProcess = spawn(process.execPath, ['-e', code]);

checkProcess.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});

checkProcess.stdout.on('data', common.mustCall((chunk) => {
  assert.strictEqual(chunk.toString(), 'x');
  checkProcess.kill(0);
  checkProcess.stdin.write('x');
  checkProcess.stdin.end();
}));
