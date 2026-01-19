'use strict';

// This tests support for the dns module in snapshot.

require('../common');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { buildSnapshot, runWithSnapshot } = require('../common/snapshot');

const entry = fixtures.path('snapshot', 'dns-lookup.js');
const env = {
  NODE_TEST_HOST: 'localhost',
  NODE_TEST_PROMISE: 'true',
};

tmpdir.refresh();
function checkOutput(stderr, stdout) {
  // We allow failures as it's not always possible to resolve localhost.
  // Functional tests are done in test/internet instead.
  if (!stderr.startsWith('error:')) {
    assert.match(stdout, /address: "\d+\.\d+\.\d+\.\d+"/);
    assert.match(stdout, /family: 4/);
    assert.strictEqual(stdout.trim().split('\n').length, 2);
  }
}
{
  const { stderr, stdout } = buildSnapshot(entry, env);
  checkOutput(stderr, stdout);
}

{
  const { stderr, stdout } = runWithSnapshot(entry, env);
  checkOutput(stderr, stdout);
}
