'use strict';

require('../common');
const assert = require('node:assert');
const { test } = require('node:test');
const {
  openAsBlob,
  openAsBlobSync,
  statSync,
  utimesSync,
  writeFileSync,
} = require('node:fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

for (const openBlob of [openAsBlob, openAsBlobSync]) {
  test(`${openBlob.name} detects whole-second mtime changes`, async () => {
    const file = tmpdir.resolve(openBlob.name);
    const time = 1_700_000_000;
    writeFileSync(file, 'before');
    utimesSync(file, time, time);
    const before = statSync(file, { bigint: true });

    const blob = await openBlob(file);
    assert.strictEqual(await blob.text(), 'before');

    writeFileSync(file, 'after!');
    utimesSync(file, time, time + 2);
    const after = statSync(file, { bigint: true });
    assert.strictEqual(after.size, before.size);
    assert.strictEqual(after.mtimeNs - before.mtimeNs, 2_000_000_000n);

    await assert.rejects(blob.text(), { name: 'NotReadableError' });
  });
}
