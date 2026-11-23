'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const testCases = [
  ['await 2', '2'],
  ['Promise.resolve(2)', 'Promise { 2 }'],
  ['await 1; Promise.resolve(2);', 'Promise { 2 }'],
  ['new Promise(() => {})', 'Promise { <pending> }'],
  ['await Promise.resolve(123), Promise.resolve(456)', 'Promise { 456 }'],
  ['await Promise.resolve(123), await Promise.resolve(456)', '456'],
  ['await (Promise.resolve(123), Promise.resolve(456))', '456'],
  ['(async () => 7)()', 'Promise { 7 }'],
  ['await (async () => 7)()', '7'],
  ['const p = Promise.resolve(8)', 'undefined'],
  ['p', 'Promise { 8 }'],
  ['await p', '8'],
];

(async () => {
  const { output, replServer, run } = startNewREPLServer({
    terminal: false,
    useColors: false,
  });

  for (const [input, expected] of testCases) {
    output.accumulator = '';
    await run(`${input}\n`);
    assert.strictEqual(output.accumulator, `${expected}\n`, input);
  }

  replServer.close();
})().then(common.mustCall());
