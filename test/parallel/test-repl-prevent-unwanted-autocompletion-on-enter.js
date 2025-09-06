'use strict';
require('../common');
const repl = require('repl');
const { describe, it } = require('node:test');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const outputStream = new ArrayStream();
let accum = '';
outputStream.write = (data) => accum += data.replace('\r', '');

function prepareREPL() {
  const replServer = repl.start({
    prompt: '',
    input: new ArrayStream(),
    output: outputStream,
    allowBlockingCompletions: true,
  });


  return replServer;
}


describe('REPL Prevent unwanted autocompletion on pressing Enter', () => {
  const code = [
    'typeof Reflect.construct',
    'typeof Reflect.const',
  ];

  it(`should not autocomplete constructor`, async () => {
    const { input } = prepareREPL();
    input.run(code);
    assert.strictEqual(accum, '\'function\'\n\'undefined\'\n');
  });
});
