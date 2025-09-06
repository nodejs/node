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
const { describe, it } = require('node:test');
const assert = require('assert');

const repl = require('repl');

function prepareREPL() {
  const input = new ArrayStream();
  const replServer = repl.start({
    prompt: '',
    input,
    output: process.stdout,
    allowBlockingCompletions: true,
  });

  // Some errors are passed to the domain, but do not callback
  replServer._domain.on('error', assert.ifError);

  return { replServer, input };
}

function getNoResultsFunction() {
  return common.mustSucceed((data) => {
    assert.deepStrictEqual(data[0], []);
  });
}

describe('REPL tab completion without side effects', () => {
  const setup = [
    'globalThis.counter = 0;',
    'function incCounter() { return counter++; }',
    'const arr = [{ bar: "baz" }];',
  ];
  // None of these expressions should affect the value of `counter`
  for (const code of [
    'incCounter().',
    'a=(counter+=1).foo.',
    'a=(counter++).foo.',
    'for((counter)of[1])foo.',
    'for((counter)in{1:1})foo.',
    'arr[incCounter()].b',
  ]) {
    it(`does not evaluate with side effects (${code})`, async () => {
      const { replServer, input } = prepareREPL();
      input.run(setup);

      replServer.complete(code, getNoResultsFunction());

      assert.strictEqual(replServer.context.counter, 0);
      replServer.close();
    });
  }
});
