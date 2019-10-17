'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const { execFile } = require('child_process');

const fixture = fixtures.path('child-process-echo-env.js');

Object.prototype.POLLUTED = 'yes!';

assert.deepStrictEqual(({}).POLLUTED, 'yes!');

execFile(
  process.execPath,
  [fixture, 'POLLUTED'],
  common.mustCall((err, stdout, stderr) => {
    assert.ifError(err);
    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout.trim(), 'undefined');
    delete Object.prototype.POLLUTED;
  })
);
