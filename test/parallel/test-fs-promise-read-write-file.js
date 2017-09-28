'use strict';

const common = require('../common');
const {
  async: fs
} = require('fs');
const assert = require('assert');
const {
  Buffer
} = require('buffer');
const {
  URL
} = require('url');
const path = require('path');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

const src = path.join(common.tmpDir, 'a.js');

async function test(path, data) {
  await fs.writeFile(path, data.slice(0, -2));
  await fs.appendFile(path, data.slice(-2));
  const content = await fs.readFile(path);
  if (typeof data === 'string') {
    assert.strictEqual(data, content.toString());
  } else {
    assert.deepStrictEqual(data, content);
  }
}

test(src, 'testing')
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(`${src}-1`, Buffer.from('testing'))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(Buffer.from(`${src}-2`), Buffer.from('testing'))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(new URL(`file://${src}-3`), Buffer.from('testing'))
  .then(common.mustCall())
  .catch(common.mustNotCall());
