'use strict';

// Flags: --expose-internals

const common = require('../common');
const stream = require('stream');
const repl = require('internal/repl');
const assert = require('assert');

// Array of [useGlobal, expectedResult] pairs
const globalTestCases = [
  [false, 'undefined'],
  [true, '\'tacos\''],
  [undefined, 'undefined']
];

const globalTest = (useGlobal, cb, output) => (err, repl) => {
  if (err)
    return cb(err);

  // The REPL registers 'module' and 'require' globals
  common.allowGlobals(repl.context.module, repl.context.require);

  let str = '';
  output.on('data', (data) => (str += data));
  global.lunch = 'tacos';
  repl.write('global.lunch;\n');
  setTimeout(() => {
    delete global.lunch;
    repl.close();
    cb(null, str.trim());
  }, 100);
};

// Test how the global object behaves in each state for useGlobal
const runGlobalTestCases = ([testCase, ...rest]) => {
  if (!testCase) return;
  const [option, expected] = testCase;
  runRepl(option, globalTest, common.mustCall((err, output) => {
    assert.ifError(err);
    assert.strictEqual(output, expected);
    runGlobalTestCases(rest);
  }));
};

runGlobalTestCases(globalTestCases);

// Test how shadowing the process object via `let`
// behaves in each useGlobal state. Note: we can't
// actually test the state when useGlobal is true,
// because the exception that's generated is caught
// (see below), but errors are printed, and the test
// suite is aware of it, causing a failure to be flagged.
//
const processTestCases = [false, undefined];
const processTest = (useGlobal, cb, output) => (err, repl) => {
  if (err)
    return cb(err);

  let str = '';
  output.on('data', (data) => (str += data));

  // if useGlobal is false, then `var process` should work
  repl.write('var process;\n');
  repl.write('21 * 2;\n');
  setTimeout(() => {
    repl.close();
    cb(null, str.trim());
  }, 100);
};


const runProcessTestCases = () => {
  const testCase = processTestCases.splice(0, 1);
  if (testCase.length) {
    const option = testCase[0];
    runRepl(option, processTest, common.mustCall((err, output) => {
      assert.ifError(err);
      assert.strictEqual(output, '42');
      runProcessTestCases();
    }));
  }
};

runProcessTestCases();

function runRepl(useGlobal, testFunc, cb) {
  const inputStream = new stream.PassThrough();
  const outputStream = new stream.PassThrough();
  const opts = {
    input: inputStream,
    output: outputStream,
    useGlobal: useGlobal,
    useColors: false,
    terminal: false,
    displayWelcomeMessage: false,
    prompt: ''
  };

  repl.createInternalRepl(
      process.env,
      opts,
      testFunc(useGlobal, cb, opts.output));
}
