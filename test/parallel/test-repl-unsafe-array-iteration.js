'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');

function run(input, expectation) {
  const inputStream = new ArrayStream();
  const outputStream = new ArrayStream();
  let output = '';

  outputStream.write = (data) => { output += data.replace('\r', ''); };

  const r = repl.start({
    input: inputStream,
    output: outputStream,
    terminal: true
  });

  r.on('exit', common.mustCall(() => {
    const actual = output.split('\n');

    // Validate that the for loop returns undefined
    assert.strictEqual(actual[actual.length - 2], expectation);
  }));

  inputStream.run(input);
  r.close();
}

run([
  'delete Array.prototype[Symbol.iterator];',
  'for(const x of [3, 2, 1]);'
], 'Uncaught TypeError: [3,2,1] is not iterable');

run([
  'const ArrayIteratorPrototype =',
  '  Object.getPrototypeOf(Array.prototype[Symbol.iterator]());',
  'delete ArrayIteratorPrototype.next;',
  'for(const x of [3, 2, 1]);'
], 'Uncaught TypeError: undefined is not a function');
