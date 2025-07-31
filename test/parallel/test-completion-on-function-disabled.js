'use strict';

const common = require('../common');
const assert = require('node:assert');
const { test } = require('node:test');


const ArrayStream = require('../common/arraystream');
const repl = require('node:repl');

function runCompletionTests(replInit, tests) {
  const stream = new ArrayStream();
  const testRepl = repl.start({ stream });

  // Some errors are passed to the domain
  testRepl._domain.on('error', assert.ifError);

  testRepl.write(replInit);
  testRepl.write('\n');

  tests.forEach(([query, expectedCompletions]) => {
    testRepl.complete(query, common.mustCall((error, data) => {
      const actualCompletions = data[0];
      if (expectedCompletions.length === 0) {
        assert.deepStrictEqual(actualCompletions, []);
      } else {
        expectedCompletions.forEach((expectedCompletion) =>
          assert(actualCompletions.includes(expectedCompletion), `completion '${expectedCompletion}' not found`)
        );
      }
    }));
  });
}

test('REPL completion on function disabled', () => {
  runCompletionTests(`
        function foo() { return { a: { b: 5 } } } 
        const obj = { a: 5 }
        const getKey = () => 'a';
        `, [
    ['foo().', []],
    ['foo().a', []],
    ['foo().a.', []],
    ['foo()["a"]', []],
    ['foo()["a"].', []],
    ['foo()["a"].b', []],
    ['obj[getKey()].', []],
  ]);

});
