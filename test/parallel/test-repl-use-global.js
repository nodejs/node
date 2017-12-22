'use strict';

// Flags: --expose-internals

const common = require('../common');
const stream = require('stream');
const repl = require('internal/repl');
const assert = require('assert');

const globalTest = (useGlobal, cb, output) => (err, repl) => {
  if (err)
    return cb(err);

  // The REPL registers 'module' and 'require' globals
  common.allowGlobals(repl.context.module, repl.context.require);

  let str = '';
  output.on('data', (data) => (str += data));
  global.lunch = 'tacos';
  repl.write('global.lunch;\n');
  repl.close();
  repl.on('exit', common.mustCall(() => {
    delete global.lunch;
    cb(null, str.trim());
  }));
};

const processTest = (useGlobal, cb, output) => (err, repl) => {
  if (err)
    return cb(err);

  let str = '';
  output.on('data', (data) => (str += data));

  // if useGlobal is false, then `let process` should work
  repl.write('let process;\n');
  repl.write('21 * 2;\n');
  repl.close();
  repl.on('exit', common.mustCall((err) => {
    assert.ifError(err);
    cb(null, str.trim());
  }));
};

// Array of [useGlobal, expectedResult, fn] pairs
const testCases = [
  // Test how the global object behaves in each state for useGlobal
  [false, 'undefined', globalTest],
  [true, '\'tacos\'', globalTest],
  [undefined, 'undefined', globalTest],
  // Test how shadowing the process object via `let`
  // behaves in each useGlobal state. Note: we can't
  // actually test the state when useGlobal is true,
  // because the exception that's generated is caught
  // (see below), but errors are printed, and the test
  // suite is aware of it, causing a failure to be flagged.
  [false, 'undefined\n42', processTest]
];

const next = common.mustCall(() => {
  if (testCases.length) {
    const [option, expected, runner] = testCases.shift();
    runRepl(option, runner, common.mustCall((err, output) => {
      assert.strictEqual(output, expected);
      next();
    }));
  }
}, testCases.length + 1);
next();

function runRepl(useGlobal, testFunc, cb) {
  const inputStream = new stream.PassThrough();
  const outputStream = new stream.PassThrough();
  const opts = {
    input: inputStream,
    output: outputStream,
    useGlobal: useGlobal,
    useColors: false,
    terminal: false,
    prompt: ''
  };

  repl.createInternalRepl(
    process.env,
    opts,
    testFunc(useGlobal, cb, opts.output));
}
