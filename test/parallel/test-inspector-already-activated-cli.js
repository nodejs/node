// Flags: --inspect=0
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIfWorker();

const assert = require('assert');
const inspector = require('inspector');
const wsUrl = inspector.url();
assert(wsUrl.startsWith('ws://'));
assert.throws(() => {
  inspector.open(0, undefined, false);
}, {
  code: 'ERR_INSPECTOR_ALREADY_ACTIVATED'
});
assert.strictEqual(inspector.url(), wsUrl);
inspector.close();
assert.strictEqual(inspector.url(), undefined);
