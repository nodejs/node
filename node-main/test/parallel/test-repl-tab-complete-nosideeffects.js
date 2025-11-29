'use strict';

const common = require('../common');
const { describe, it } = require('node:test');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

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
      const { replServer, input } = startNewREPLServer();
      input.run(setup);

      replServer.complete(code, getNoResultsFunction());

      assert.strictEqual(replServer.context.counter, 0);
      replServer.close();
    });
  }
});
