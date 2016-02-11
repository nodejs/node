'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

{
  const options = {
    eval: common.mustCall((cmd, context) => {
      assert.strictEqual(cmd, 'foo\n');
      assert.deepStrictEqual(context, {animal: 'Sterrance'});
    })
  };

  const r = repl.start(options);
  r.context = {animal: 'Sterrance'};

  try {
    r.write('foo\n');
  } finally {
    r.write('.exit\n');
  }
}
