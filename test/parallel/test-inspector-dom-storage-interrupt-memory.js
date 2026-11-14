'use strict';
const common = require('../common');
common.skipIfSQLiteMissing();
common.skipIfInspectorDisabled();

// Both stores in memory and never touched, read from a V8 API interrupt.
const assert = require('assert');
const { getItemsWhileBusy } = require('../common/inspector-dom-storage');

(async () => {
  const results = await getItemsWhileBusy(':memory:', '', [true, false]);
  assert.deepStrictEqual(results.map((r) => r.entries), [[], []]);
})().then(common.mustCall());
