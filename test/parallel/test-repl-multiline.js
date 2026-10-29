'use strict';
const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');
const { stripVTControlCharacters } = require('util');

const input = ['const foo = {', '};', 'foo'];

async function run({ useColors }) {
  const { replServer, output, run: write } = startNewREPLServer({ useColors });

  await write(input);

  // The output contains various escape codes, including
  // screen clears and others, so we trim them all to
  // simplify this test.
  const actual = stripVTControlCharacters(output.accumulator);

  const firstStatementIdx = actual.indexOf(input.slice(0, 1).join('\n| '));
  assert(firstStatementIdx > -1);

  assert(actual.slice(firstStatementIdx).includes('undefined'));
  assert(actual.slice(firstStatementIdx).includes('foo'));

  replServer.on('exit', common.mustCall());
  replServer.close();
}

run({ useColors: true })
  .then(() => run({ useColors: false }))
  .then(common.mustCall());
