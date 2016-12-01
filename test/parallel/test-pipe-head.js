'use strict';
const common = require('../common');
const assert = require('assert');

const exec = require('child_process').exec;
const join = require('path').join;

const nodePath = process.argv[0];
const script = join(common.fixturesDir, 'print-10-lines.js');

const cmd = '"' + nodePath + '" "' + script + '" | head -2';

exec(cmd, common.mustCall(function(err, stdout, stderr) {
  if (err) throw err;
  const lines = stdout.split('\n');
  assert.strictEqual(3, lines.length);
}));
