'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that node doesn't SEGFAULT when either of
// `crypto.createHash` or `crypto.createHmac` are given an object that defines
// a throwing `toString`.
// https://github.com/nodejs/node/issues/9819

const assert = require('assert');
const execFile = require('child_process').execFile;

const setup = 'const enc = { toString: () => { throw new Error("xyz"); } };';

const scripts = [
  'crypto.createHash("sha256").digest(enc)',
  'crypto.createHmac("sha256", "msg").digest(enc)'
];

scripts.forEach((script) => {
  const node = process.execPath;
  const code = `${setup};${script}`;
  execFile(node, [ '-e', code ], common.mustCall((err, stdout, stderr) => {
    assert(stderr.includes('Error: xyz'), 'digest crashes');
  }));
});
