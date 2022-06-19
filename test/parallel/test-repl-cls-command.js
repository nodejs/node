'use strict';

require('../common');
const assert = require('assert');
const repl = require('repl');
const ArrayStream = require('../common/arraystream');

const clearChar = /\[0;0f>/;
let accum = '';
const output = new ArrayStream();
output.write = (data) => (accum += data.replace('\r', ''));

const r = repl.start({
  input: new ArrayStream(),
  output,
});
r.write('.help\n');
assert.strictEqual(accum.match(clearChar), null);
r.write('.cls\n');
assert.strictEqual(accum.match(clearChar).length > 0, true);
r.write('.exit\n');
