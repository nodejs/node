'use strict';

// This tests support for the dns module in snapshot.

require('../common');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { buildSnapshot, runWithSnapshot } = require('../common/snapshot');
const {
  addresses: { DNS4_SERVER, INET4_IP, INET4_HOST },
} = require('../common/internet');

const entry = fixtures.path('snapshot', 'dns-resolve.js');
const env = {
  NODE_TEST_IP: INET4_IP,
  NODE_TEST_HOST: INET4_HOST,
  NODE_TEST_DNS: DNS4_SERVER,
  NODE_TEST_PROMISE: 'false',
};

tmpdir.refresh();
function checkOutput(stderr, stdout) {
  assert(stdout.includes('hostnames: ['));
  assert(stdout.includes('addresses: ['));
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
