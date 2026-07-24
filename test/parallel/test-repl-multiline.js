'use strict';
const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const input = ['const foo = {', '};', 'foo'];

async function run({ useColors }) {
  const { replServer, output, run: write } = startNewREPLServer({ useColors });

  await write(input);

  const actual = output.accumulator.split('\n');

  // Validate the output, which contains terminal escape codes.
  assert.strictEqual(actual.length, 6);
  assert.ok(actual[0].endsWith(input[0]));
  assert.ok(actual[1].includes('| '));
  assert.ok(actual[1].endsWith(input[1]));
  assert.ok(actual[2].includes('undefined'));
  assert.ok(actual[3].endsWith(input[2]));
  assert.strictEqual(actual[4], '{}');

  replServer.on('exit', common.mustCall());
  replServer.close();
}

run({ useColors: true })
  .then(() => run({ useColors: false }))
  .then(common.mustCall());
