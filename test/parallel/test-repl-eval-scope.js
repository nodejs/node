'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

{
  const stream = new common.ArrayStream();
  const options = {
    eval: common.mustCall((cmd, context) => {
      assert.strictEqual(cmd, '.scope\n');
      assert.deepStrictEqual(context, { animal: 'Sterrance' });
    }),
    input: stream,
    output: stream,
    terminal: true
  };

  const r = repl.start(options);
  r.context = { animal: 'Sterrance' };

  stream.emit('data', '\t');
  stream.emit('.exit\n');
}
