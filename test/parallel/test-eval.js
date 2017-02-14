'use strict';
const common = require('../common');
const util = require('util');
const assert = require('assert');
const exec = require('child_process').exec;

const cmd = [
  `"${process.execPath}"`, '-e',
  '"console.error(process.argv)"',
  'foo', 'bar'].join(' ');
const expected = util.format([process.execPath, 'foo', 'bar']) + '\n';
exec(cmd, common.mustCall((err, stdout, stderr) => {
  assert.ifError(err);
  assert.strictEqual(stderr, expected);
}));
