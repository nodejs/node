'use strict';
const common = require('../common');

// This test is only relevant on Windows.
if (!common.isWindows)
  common.skip('Windows specific test.');

// This test ensures fs.realpathSync works on properly on Windows without
// throwing ENOENT when the path involves a fileserver.
// https://github.com/nodejs/node-v0.x-archive/issues/3542

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');

function test(p) {
  const result = fs.realpathSync(p);
  assert.strictEqual(result.toLowerCase(), path.resolve(p).toLowerCase());

  fs.realpath(p, common.mustCall(function(err, result) {
    assert.ok(!err);
    assert.strictEqual(result.toLowerCase(), path.resolve(p).toLowerCase());
  }));
}

test(`//${os.hostname()}/c$/Windows/System32`);
test(`//${os.hostname()}/c$/Windows`);
test(`//${os.hostname()}/c$/`);
test(`\\\\${os.hostname()}\\c$\\`);
test('C:\\');
test('C:');
test(process.env.windir);
