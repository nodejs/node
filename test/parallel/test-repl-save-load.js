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
const { startNewREPLServer } = require('../common/repl');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Tests that a REPL session data can be saved to and loaded from a file

const { replServer, run } = startNewREPLServer({ terminal: false });

const filePath = path.resolve(tmpdir.path, 'test.save.js');

const testFileContents = [
  'let inner = (function() {',
  '  return {one:1};',
  '})()',
];

const innerOCompletions = [['inner.one'], 'inner.o'];

// Evaluation is asynchronous (the REPL now drives the inspector), so completion
// must be awaited as well.
function complete(text) {
  return new Promise((resolve) => {
    replServer.completer(text, common.mustSucceed(resolve));
  });
}

async function main() {
  await run(testFileContents);
  await run([`.save ${filePath}`]);

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'),
                     testFileContents.join('\n'));

  // Double check that the data is still present in the repl after the save
  assert.deepStrictEqual(await complete('inner.o'), innerOCompletions);

  // Clear the repl context
  await run(['.clear']);

  // Double check that the data is no longer present in the repl
  assert.deepStrictEqual(await complete('inner.o'), [[], 'inner.o']);

  // Load the file back in.
  await run([`.load ${filePath}`]);

  // Make sure loading doesn't insert extra indentation
  // https://github.com/nodejs/node/issues/47673
  assert.strictEqual(replServer.line, '');

  // Make sure that the loaded data is present
  assert.deepStrictEqual(await complete('inner.o'), innerOCompletions);

  replServer.close();
}

main().then(common.mustCall());
