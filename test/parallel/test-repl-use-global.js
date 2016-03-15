'use strict';

// Flags: --expose-internals

const common = require('../common');
const stream = require('stream');
const repl = require('internal/repl');
const assert = require('assert');

common.globalCheck = false;

runRepl(false, common.mustCall(function(err, output) {
  assert.ifError(err);
  assert.strictEqual(output, 'undefined');

  runRepl(true, common.mustCall(function(err, output) {
    assert.ifError(err);
    assert.strictEqual(output, '\'tacos\'');

    runRepl(undefined, common.mustCall(function(err, output) {
      assert.ifError(err);
      assert.strictEqual(output, 'undefined');
    }));
  }));
}));

function runRepl(useGlobal, cb) {
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

  let output = '';

  outputStream.on('data', (data) => {
    output += data;
  });

  repl.createInternalRepl(process.env, opts, function(err, repl) {
    if (err)
      return cb(err);

    global.lunch = 'tacos';
    repl.write('global.lunch;\n');
    repl.close();
    delete global.lunch;
    cb(null, output.trim());
  });
}
