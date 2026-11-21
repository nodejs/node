'use strict';
const common = require('../common');
common.skipIfSQLiteMissing();
common.skipIfInspectorDisabled();

// Both stores in memory, read from a V8 API interrupt after the application
// created sessionStorage first. Each read must return its own store.
const assert = require('assert');
const { getItemsWhileBusy } = require('../common/inspector-dom-storage');

(async () => {
  const [local, session] = await getItemsWhileBusy(':memory:', `
    sessionStorage.setItem('s', 'session');
    localStorage.setItem('l', 'local');
  `, [true, false]);
  assert.deepStrictEqual(local.entries, [['l', 'local']]);
  assert.deepStrictEqual(session.entries, [['s', 'session']]);
})().then(common.mustCall());
