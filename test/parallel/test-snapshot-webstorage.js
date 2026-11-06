'use strict';

// Tests that web storage can be touched by a snapshot builder script: the
// binding can be serialized, creating a Storage is rejected with an error, and
// the options passed after deserialization are honored.

const common = require('../common');
common.skipIfSQLiteMissing();
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const {
  spawnSyncAndAssert,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');

for (const [name, args] of [
  ['localStorage', [`--localstorage-file=${tmpdir.resolve('build.db')}`]],
  ['sessionStorage', []],
]) {
  const entry = tmpdir.resolve(`${name}.js`);
  fs.writeFileSync(entry, `globalThis.${name};\n`);
  spawnSyncAndAssert(process.execPath, [
    ...args,
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    entry,
  ], {
    cwd: tmpdir.path,
  }, {
    status: 1,
    stderr: new RegExp(
      `ERR_NOT_SUPPORTED_IN_SNAPSHOT.*${name} is not supported in startup snapshot`, 's'),
  });
  assert(!fs.existsSync(blobPath));
}

{
  const entry = tmpdir.resolve('no-file.js');
  fs.writeFileSync(entry, `
    const { startupSnapshot } = require('v8');
    if (globalThis.localStorage !== undefined) throw new Error('unexpected');
    // Registered after the web storage module has registered its own.
    startupSnapshot.addSerializeCallback(() => globalThis.localStorage);
    startupSnapshot.setDeserializeMainFunction(() => {
      localStorage?.setItem('local', 'a');
      sessionStorage.setItem('session', 'b');
      console.log(localStorage?.getItem('local'), sessionStorage.getItem('session'));
    });
  `);
  spawnSyncAndExitWithoutError(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    entry,
  ], {
    cwd: tmpdir.path,
  });

  spawnSyncAndAssert(process.execPath, [
    `--localstorage-file=${tmpdir.resolve('run.db')}`,
    '--snapshot-blob',
    blobPath,
  ], {
    cwd: tmpdir.path,
  }, {
    stdout: 'a b',
    trim: true,
  });

  // The warning from the build is not carried over into the snapshot.
  spawnSyncAndAssert(process.execPath, [
    '--snapshot-blob',
    blobPath,
  ], {
    cwd: tmpdir.path,
  }, {
    stdout: 'undefined b',
    stderr: /localStorage is not available because --localstorage-file was not provided/,
    trim: true,
  });
}
