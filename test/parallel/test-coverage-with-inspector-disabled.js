'use strict';

const common = require('../common');
if (process.features.inspector) {
  common.skip('V8 inspector is enabled');
}

const fixtures = require('../common/fixtures');
const assert = require('assert');
const { spawnSync } = require('child_process');
const env = { ...process.env, NODE_V8_COVERAGE: '/foo/bar' };
const childPath = fixtures.path('v8-coverage/subprocess');
const { status, stderr } = spawnSync(
  process.execPath,
  [childPath],
  { env }
);

const warningMessage = 'The inspector is disabled, ' +
                        'coverage could not be collected';

assert.strictEqual(status, 0);
assert.strictEqual(
  stderr.toString().includes(`Warning: ${warningMessage}`),
  true
);
