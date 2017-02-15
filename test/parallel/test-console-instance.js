'use strict';
const common = require('../common');
const assert = require('assert');
const Stream = require('stream');
const Console = require('console').Console;

const out = new Stream();
const err = new Stream();

// ensure the Console instance doesn't write to the
// process' "stdout" or "stderr" streams
process.stdout.write = process.stderr.write = common.mustNotCall();

// make sure that the "Console" function exists
assert.strictEqual('function', typeof Console);

// make sure that the Console constructor throws
// when not given a writable stream instance
assert.throws(() => {
  new Console();
}, /^TypeError: Console expects a writable stream instance$/);

// Console constructor should throw if stderr exists but is not writable
assert.throws(() => {
  out.write = () => {};
  err.write = undefined;
  new Console(out, err);
}, /^TypeError: Console expects writable stream instances$/);

out.write = err.write = (d) => {};

const c = new Console(out, err);

out.write = err.write = common.mustCall((d) => {
  assert.strictEqual(d, 'test\n');
}, 2);

c.log('test');
c.error('test');

out.write = common.mustCall((d) => {
  assert.strictEqual('{ foo: 1 }\n', d);
});

c.dir({ foo: 1 });

// ensure that the console functions are bound to the console instance
let called = 0;
out.write = common.mustCall((d) => {
  called++;
  assert.strictEqual(d, `${called} ${called - 1} [ 1, 2, 3 ]\n`);
}, 3);

[1, 2, 3].forEach(c.log);

// Console() detects if it is called without `new` keyword
assert.doesNotThrow(() => {
  Console(out, err);
});
