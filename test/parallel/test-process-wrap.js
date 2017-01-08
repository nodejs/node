'use strict';
require('../common');
const assert = require('assert');
const Process = process.binding('process_wrap').Process;
const Pipe = process.binding('pipe_wrap').Pipe;
const pipe = new Pipe();
const p = new Process();

let processExited = false;
let gotPipeEOF = false;
let gotPipeData = false;

p.onexit = function(exitCode, signal) {
  console.log('exit');
  p.close();
  pipe.readStart();

  assert.strictEqual(0, exitCode);
  assert.strictEqual('', signal);

  processExited = true;
};

pipe.onread = function(err, b, off, len) {
  assert.ok(processExited);
  if (b) {
    gotPipeData = true;
    console.log('read %d', len);
  } else {
    gotPipeEOF = true;
    pipe.close();
  }
};

p.spawn({
  file: process.execPath,
  args: [process.execPath, '-v'],
  stdio: [
    { type: 'ignore' },
    { type: 'pipe', handle: pipe },
    { type: 'ignore' }
  ]
});

// 'this' safety
// https://github.com/joyent/node/issues/6690
assert.throws(function() {
  const notp = { spawn: p.spawn };
  notp.spawn({
    file: process.execPath,
    args: [process.execPath, '-v'],
    stdio: [
      { type: 'ignore' },
      { type: 'pipe', handle: pipe },
      { type: 'ignore' }
    ]
  });
}, TypeError);

process.on('exit', function() {
  assert.ok(processExited);
  assert.ok(gotPipeEOF);
  assert.ok(gotPipeData);
});
