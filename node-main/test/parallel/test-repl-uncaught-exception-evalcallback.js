'use strict';
const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const { replServer, output } = startNewREPLServer(
  {
    prompt: '',
    terminal: false,
    useColors: false,
    global: false,
    eval: common.mustCall((code, context, filename, cb) => {
      replServer.setPrompt('prompt! ');
      cb(new Error('err'));
    })
  },
  {
    disableDomainErrorAssert: true
  },
);

replServer.write('foo\n');

// The output includes exactly one post-error prompt.
assert.match(output.accumulator, /prompt!/);
assert.doesNotMatch(output.accumulator, /prompt![\S\s]*prompt!/);
output.on('data', common.mustNotCall());
