'use strict';

// This tests support for the dns module in snapshot.

require('../common');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { buildSnapshot, runWithSnapshot } = require('../common/snapshot');
const {
  addresses: { INET4_HOST },
} = require('../common/internet');

const entry = fixtures.path('snapshot', 'dns-lookup.js');
const env = {
  NODE_TEST_HOST: INET4_HOST,
  NODE_TEST_PROMISE: 'false',
};

tmpdir.refresh();
function checkOutput(stderr, stdout) {
  assert.match(stdout, /address: "\d+\.\d+\.\d+\.\d+"/);
  assert.match(stdout, /family: 4/);
  assert.strictEqual(stdout.trim().split('\n').length, 2);
}
{
  const { stderr, stdout } = buildSnapshot(entry, env);
  checkOutput(stderr, stdout);
}

{
  const { stderr, stdout } = runWithSnapshot(entry, env);
  checkOutput(stderr, stdout);
}
