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
const ArrayStream = require('../common/arraystream');

const assert = require('node:assert');
const fs = require('node:fs');
const repl = require('node:repl');
const path = require('node:path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Tests that a REPL session data can be saved to and loaded from a file

const input = new ArrayStream();

const replServer = repl.start({
  prompt: '',
  input,
  output: new ArrayStream(),
  allowBlockingCompletions: true,
});

// Some errors are passed to the domain, but do not callback
replServer._domain.on('error', assert.ifError);

const filePath = path.resolve(tmpdir.path, 'test.save.js');

const testFileContents = [
  'let inner = (function() {',
  '  return {one:1};',
  '})()',
];

input.run(testFileContents);
input.run([`.save ${filePath}`]);

assert.strictEqual(fs.readFileSync(filePath, 'utf8'),
                   testFileContents.join('\n'));

const innerOCompletions = [['inner.one'], 'inner.o'];

// Double check that the data is still present in the repl after the save
replServer.completer('inner.o', common.mustSucceed((data) => {
  assert.deepStrictEqual(data, innerOCompletions);
}));

// Clear the repl context
input.run(['.clear']);

// Double check that the data is no longer present in the repl
replServer.completer('inner.o', common.mustSucceed((data) => {
  assert.deepStrictEqual(data, [[], 'inner.o']);
}));

// Load the file back in.
input.run([`.load ${filePath}`]);

// Make sure loading doesn't insert extra indentation
// https://github.com/nodejs/node/issues/47673
assert.strictEqual(replServer.line, '');

// Make sure that the loaded data is present
replServer.complete('inner.o', common.mustSucceed((data) => {
  assert.deepStrictEqual(data, innerOCompletions);
}));

replServer.close();
