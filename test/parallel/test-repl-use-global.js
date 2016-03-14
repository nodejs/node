'use strict';

// Flags: --expose-internals

const common = require('../common');
const stream = require('stream');
const REPL = require('internal/repl');
const assert = require('assert');

common.globalCheck = false;

test();

function test() {

  runRepl(false, (err, output) => {
    assert.ifError(err);
    assert.strictEqual(output, 'undefined');

    runRepl(true, (err, output) => {
      assert.ifError(err);
      assert.strictEqual(output, '\'tacos\'');
    });
  });
}

function runRepl(useGlobal, cb) {
  const inputStream = new stream.PassThrough();
  const outputStream = new stream.PassThrough();

  const opts = {
    input: inputStream,
    output: outputStream,
    useColors: false,
    terminal: false,
    prompt: ''
  };

  // Don't blindly set useGlobal, because we want to test
  // the default case as well, where useGlobal is not provided.
  if (useGlobal)
    opts.useGlobal = true;

  let output = '';

  outputStream.on('data', (data) => {
    output += data;
  });

  REPL.createInternalRepl(process.env, opts, function(err, repl) {

    if (err)
      return cb(err);

    global.lunch = 'tacos';
    repl.write('global.lunch;\n');
    repl.close();
    delete global.lunch;
    cb(null, output.trim());
  });
}
