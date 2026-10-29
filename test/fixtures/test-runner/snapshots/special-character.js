'use strict';
const { snapshot, test } = require('node:test');
const { basename, join } = require('node:path');

snapshot.setResolveSnapshotPath((testFile) => {
  return join(process.cwd(), `${basename(testFile)}.snapshot`);
});

test('\r', (t) => {
  t.assert.snapshot({ key: 'value' });
});

test(String.fromCharCode(55296), (t) => {
  t.assert.snapshot({ key: 'value' });
});

test(String.fromCharCode(57343), (t) => {
  t.assert.snapshot({ key: 'value' });
});
