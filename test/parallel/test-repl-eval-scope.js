'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');

const exitTests = [];
process.on('exit', () => {
  for (const test of exitTests) test();
});
const CONTEXT = { animal: 'Sterrance' };
const stream = new common.ArrayStream();
const options = {
  eval: common.mustCall((cmd, context) => {
    // need to escape the domain
    exitTests.push(common.mustCall(() => {
      assert.strictEqual(cmd, '.scope');
      assert.ok(context === CONTEXT);
    }));
  }),
  input: stream,
  output: stream,
  terminal: true
};

const r = repl.start(options);
r.context = CONTEXT;

stream.emit('data', '\t');
stream.emit('.exit\n');
