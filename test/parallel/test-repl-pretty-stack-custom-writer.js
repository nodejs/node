'use strict';
require('../common');
const { PassThrough } = require('stream');
const assert = require('assert');
const repl = require('repl');

{
  const input = new PassThrough();
  const output = new PassThrough();

  const r = repl.start({
    prompt: '',
    input,
    output,
    writer: String,
    terminal: false,
    useColors: false
  });

  r.write('throw new Error("foo[a]")\n');
  r.close();
  assert.strictEqual(output.read().toString(), 'Uncaught Error: foo[a]\n');
}
