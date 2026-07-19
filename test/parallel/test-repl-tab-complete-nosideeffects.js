'use strict';

require('../common');
const { describe, it } = require('node:test');
const assert = require('assert');
const { startNewREPLServer, complete } = require('../common/repl');

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
      const { replServer, run } = startNewREPLServer();
      await run(setup);

      const data = await complete(replServer, code);
      assert.deepStrictEqual(data[0], []);

      assert.strictEqual(replServer.context.counter, 0);
      replServer.close();
    });
  }
});
