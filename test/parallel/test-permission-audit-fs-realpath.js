// Flags: --permission-audit
'use strict';

const common = require('../common');
const assert = require('assert');
const dc = require('diagnostics_channel');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const file = fixtures.path('permission', 'deny', 'protected-file.md');

const denied = [];
dc.subscribe('node:permission-model:fs', common.mustCallAtLeast((message) => {
  assert.strictEqual(message.permission, 'FileSystemRead');
  denied.push(message.resource);
}));

assert.strictEqual(fs.realpathSync(file), file);
assert.ok(denied.includes(file), `realpathSync reported ${file}`);
