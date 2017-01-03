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
const os = require('os');

if (!common.isSunOS) {
  common.skip('no DTRACE support');
  return;
}

/*
 * Some functions to create a recognizable stack.
 */
const frames = [ 'stalloogle', 'bagnoogle', 'doogle' ];

const stalloogle = function(str) {
  global.expected = str;
  os.loadavg();
};

const bagnoogle = function(arg0, arg1) {
  stalloogle(arg0 + ' is ' + arg1 + ' except that it is read-only');
};

let done = false;

const doogle = function() {
  if (!done)
    setTimeout(doogle, 10);

  bagnoogle('The bfs command', '(almost) like ed(1)');
};

const spawn = require('child_process').spawn;

/*
 * We're going to use DTrace to stop us, gcore us, and set us running again
 * when we call getloadavg() -- with the implicit assumption that our
 * deepest function is the only caller of os.loadavg().
 */
const dtrace = spawn('dtrace', [ '-qwn', 'syscall::getloadavg:entry/pid == ' +
  process.pid + '/{ustack(100, 8192); exit(0); }' ]);

let output = '';

dtrace.stderr.on('data', function(data) {
  console.log('dtrace: ' + data);
});

dtrace.stdout.on('data', function(data) {
  output += data;
});

dtrace.on('exit', function(code) {
  if (code !== 0) {
    console.error('dtrace exited with code ' + code);
    process.exit(code);
  }

  done = true;

  const sentinel = '(anon) as ';
  const lines = output.split('\n');

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];

    if (line.indexOf(sentinel) === -1 || frames.length === 0)
      continue;

    const frame = line.substr(line.indexOf(sentinel) + sentinel.length);
    const top = frames.shift();

    assert.strictEqual(frame.indexOf(top), 0, 'unexpected frame where ' +
      top + ' was expected');
  }

  assert.strictEqual(frames.length, 0,
                     'did not find expected frame ' + frames[0]);
  process.exit(0);
});

setTimeout(doogle, 10);
