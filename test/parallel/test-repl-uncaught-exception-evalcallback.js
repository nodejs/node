'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');
const { PassThrough } = require('stream');
const input = new PassThrough();
const output = new PassThrough();

const r = repl.start({
  input, output,
  eval: common.mustCall((code, context, filename, cb) => {
    r.setPrompt('prompt! ');
    cb(new Error('err'));
  })
});

input.end('foo\n');

// The output includes exactly one post-error prompt.
const out = output.read().toString();
assert.match(out, /prompt!/);
assert.doesNotMatch(out, /prompt![\S\s]*prompt!/);
output.on('data', common.mustNotCall());
