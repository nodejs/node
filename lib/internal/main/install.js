'use strict';

const {
  ArrayPrototypeMap,
  JSONParse,
  ObjectEntries,
  SafePromiseAllReturnVoid,
  StringPrototypeSlice,
} = primordials;

const { Buffer } = require('buffer');
const { readFileSync } = require('fs');
const { resolve } = require('path');
const {
  codes: {
    ERR_MANIFEST_INTEGRITY_MISMATCH,
  },
} = require('internal/errors');

const {
  prepareMainThreadExecution,
  markBootstrapComplete,
} = require('internal/process/pre_execution');

const lockFilePath = prepareMainThreadExecution(true);
markBootstrapComplete();

const { packages } = JSONParse(readFileSync(lockFilePath, 'utf-8'));

if (!packages) {
  return;
}

const { download } = require('internal/deps/corepack/dist/lib/download');
SafePromiseAllReturnVoid(ArrayPrototypeMap(ObjectEntries(packages), async ({ 0: path, 1: { resolved, integrity } }) => {
  const targetDir = resolve(lockFilePath, `../${path}`);
  const { tmpFolder, outputFile, hash } = await download(`${targetDir}/..`, resolved, 'sha512');
  if (Buffer.from(hash, 'hex').toString('base64') !== StringPrototypeSlice(integrity, 7)) {
    throw ERR_MANIFEST_INTEGRITY_MISMATCH();
  }
  const { rename } = require('fs/promises');
  await rename(outputFile ?? tmpFolder, targetDir);
}));
