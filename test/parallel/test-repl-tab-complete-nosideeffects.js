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
