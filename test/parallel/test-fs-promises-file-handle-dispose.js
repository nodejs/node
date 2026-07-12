'use strict';

const common = require('../common');
const assert = require('assert');
const { opendirSync, promises: fs } = require('fs');

async function explicitCall() {
  const fh = await fs.open(__filename);
  fh.on('close', common.mustCall());
  await fh[Symbol.asyncDispose]();

  const dh = await fs.opendir(__dirname);
  await dh[Symbol.asyncDispose]();
  // Repeat invocations should not reject
  await dh[Symbol.asyncDispose]();
  await assert.rejects(dh.read(), { code: 'ERR_DIR_CLOSED' });

  const dhSync = opendirSync(__dirname);
  dhSync[Symbol.dispose]();
  // Repeat invocations should not throw
  dhSync[Symbol.dispose]();
  assert.throws(() => dhSync.readSync(), { code: 'ERR_DIR_CLOSED' });
}

async function implicitCall() {
  let fh;
  {
    await using openHandle = await fs.open(__filename);
    fh = openHandle;
    fh.on('close', common.mustCall());
  }
  assert.strictEqual(fh.fd, -1);

  let dh;
  {
    await using dirHandle = await fs.opendir(__dirname);
    dh = dirHandle;
  }
  await assert.rejects(dh.read(), { code: 'ERR_DIR_CLOSED' });

  let dhSync;
  {
    using dirHandleSync = opendirSync(__dirname);
    dhSync = dirHandleSync;
  }
  assert.throws(() => dhSync.readSync(), { code: 'ERR_DIR_CLOSED' });
}

explicitCall().then(common.mustCall());
implicitCall().then(common.mustCall());
