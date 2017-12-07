'use strict';
const common = require('../common');

// Test that fs.readFile fails correctly on a non-existent file.

// `fs.readFile('/')` does not fail on FreeBSD, because you can open and read
// the directory there.
if (common.isFreeBSD)
  common.skip('platform not supported.');

const assert = require('assert');
const exec = require('child_process').exec;
const fixtures = require('../common/fixtures');

function test(env, cb) {
  const filename = fixtures.path('test-fs-readfile-error.js');
  const execPath = `"${process.execPath}" "${filename}"`;
  const options = { env: Object.assign({}, process.env, env) };
  exec(execPath, options, common.mustCall((err, stdout, stderr) => {
    assert(err);
    assert.strictEqual(stdout, '');
    assert.notStrictEqual(stderr, '');
    cb(String(stderr));
  }));
}

test({ NODE_DEBUG: '' }, common.mustCall((data) => {
  assert(/EISDIR/.test(data));
  assert(!/test-fs-readfile-error/.test(data));
}));

test({ NODE_DEBUG: 'fs' }, common.mustCall((data) => {
  assert(/EISDIR/.test(data));
  assert(/test-fs-readfile-error/.test(data));
}));
