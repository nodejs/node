'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');

const assert = require('node:assert');
const fs = require('node:fs/promises');
const { pathToFileURL } = require('node:url');

tmpdir.refresh();
const tmpDir = pathToFileURL(tmpdir.path);

const target = new URL(`./${Math.random()}.mjs`, tmpDir);

(async () => {

  await assert.rejects(import(target), { code: 'ERR_MODULE_NOT_FOUND' });

  await fs.writeFile(target, 'export default "actual target"\n');

  const moduleRecord = await import(target);

  await fs.rm(target);

  assert.strictEqual(await import(target), moduleRecord);
})().then(common.mustCall());
