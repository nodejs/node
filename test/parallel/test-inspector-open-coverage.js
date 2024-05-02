'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

common.skipIfInspectorDisabled();
common.skipIfWorker();

tmpdir.refresh();


let output = spawnSync(process.execPath, [fixtures.path('inspector-open.js')]);
assert.strictEqual(output.status, 0);

output = spawnSync(process.execPath, [fixtures.path('inspector-open.js')], {
  env: { ...process.env, NODE_V8_COVERAGE: tmpdir.path },
});
assert.strictEqual(output.status, 0);
