'use strict';
const common = require('../common');
common.skipIfSQLiteMissing();
common.skipIfInspectorDisabled();

// A localStorage file that cannot be opened, read from a V8 API interrupt, is
// reported without creating a JavaScript error, whether or not the
// application touched the store.
const assert = require('assert');
const {
  getItemsWhileBusy,
  malformedLocalStorage,
} = require('../common/inspector-dom-storage');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

(async () => {
  for (const [name, prelude] of [['untouched.db', ''],
                                 ['touched.db', 'globalThis.localStorage;']]) {
    const file = malformedLocalStorage(
      name, 'one', Buffer.from('hello', 'utf16le'));
    const [err] = await getItemsWhileBusy(file, prelude, [true]);
    assert.strictEqual(
      err.message,
      'Could not read DOM storage items: Uncaught Error: localStorage ' +
      'database is malformed: expected schema_version to be an integer');
  }
})().then(common.mustCall());
