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
require('../common');

// Open a chain of five Node processes each a child of the next. The final
// process exits immediately. Each process in the chain is instructed to exit
// when its child exits.
// https://github.com/joyent/node/issues/1726

const assert = require('assert');
const ch = require('child_process');

const gen = +(process.argv[2] || 0);
const maxGen = 5;


if (gen === maxGen) {
  console.error('hit maxGen, exiting', maxGen);
  return;
}

const child = ch.spawn(process.execPath, [__filename, gen + 1], {
  stdio: [ 'ignore', 'pipe', 'ignore' ]
});
assert.ok(!child.stdin);
assert.ok(child.stdout);
assert.ok(!child.stderr);

console.error('gen=%d, pid=%d', gen, process.pid);

/*
var timer = setTimeout(function() {
  throw new Error('timeout! gen='+gen);
}, 1000);
*/

child.on('exit', function(code) {
  console.error('exit %d from gen %d', code, gen + 1);
});

child.stdout.pipe(process.stdout);

child.stdout.on('close', function() {
  console.error('child.stdout close  gen=%d', gen);
});
