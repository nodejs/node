'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;
const path = require('path');

// `fs.readFile('/')` does not fail on FreeBSD, because you can open and read
// the directory there.
if (process.platform === 'freebsd') {
  common.skip('platform not supported.');
  return;
}

function test(env, cb) {
  const filename = path.join(common.fixturesDir, 'test-fs-readfile-error.js');
  const execPath = '"' + process.execPath + '" "' + filename + '"';
  const options = { env: Object.assign(process.env, env) };
  exec(execPath, options, common.mustCall((err, stdout, stderr) => {
    assert(err);
    assert.strictEqual(stdout, '');
    assert.notStrictEqual(stderr, '');
    cb('' + stderr);
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
