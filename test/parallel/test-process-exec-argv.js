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
const { Worker, isMainThread } = require('worker_threads');

if (process.argv[2] === 'child' || !isMainThread) {
  if (process.argv[3] === 'cp+worker')
    new Worker(__filename);
  else
    process.stdout.write(JSON.stringify(process.execArgv));
} else {
  for (const extra of [ [], [ '--' ] ]) {
    for (const kind of [ 'cp', 'worker', 'cp+worker' ]) {
      const execArgv = ['--pending-deprecation'];
      const args = [__filename, 'child', kind];
      let child;
      switch (kind) {
        case 'cp':
          child = spawn(process.execPath, [...execArgv, ...extra, ...args]);
          break;
        case 'worker':
          child = new Worker(__filename, {
            execArgv: [...execArgv, ...extra],
            stdout: true
          });
          break;
        case 'cp+worker':
          child = spawn(process.execPath, [...execArgv, ...args]);
          break;
      }

      let out = '';
      child.stdout.setEncoding('utf8');
      child.stdout.on('data', (chunk) => {
        out += chunk;
      });

      child.stdout.on('end', common.mustCall(() => {
        assert.deepStrictEqual(JSON.parse(out), execArgv);
      }));
    }
  }
}
