'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');

const assert = require('node:assert');
const fs = require('node:fs/promises');

tmpdir.refresh();
const target = tmpdir.fileURL(`${Math.random()}.mjs`);

(async () => {

  await assert.rejects(import(target), { code: 'ERR_MODULE_NOT_FOUND' });

  await fs.writeFile(target, 'export default "actual target"\n');

  const moduleRecord = await import(target);

  await fs.rm(target);

  assert.strictEqual(await import(target), moduleRecord);
})().then(common.mustCall());
