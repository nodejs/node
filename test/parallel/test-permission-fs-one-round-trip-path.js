'use strict';

// fs.readFile() and fs.writeFile() with a path open the file in a single
// thread pool job. The permission check must apply to the path the job opens,
// even when a Buffer path is changed after the job has been created.

require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const allowedRead = tmpdir.resolve('public-read');
const deniedRead = tmpdir.resolve('secret-read');
const allowedWrite = tmpdir.resolve('public-write');
const deniedWrite = tmpdir.resolve('secret-write');
fs.writeFileSync(allowedRead, 'public');
fs.writeFileSync(deniedRead, 'secret');
fs.writeFileSync(allowedWrite, 'public');
// The swapped path must fit into the Buffer that held the denied path.
assert.strictEqual(Buffer.byteLength(allowedRead), Buffer.byteLength(deniedRead));
assert.strictEqual(Buffer.byteLength(allowedWrite), Buffer.byteLength(deniedWrite));

const script = `
const assert = require('node:assert');
const fs = require('node:fs');

// Starts an operation on a denied Buffer path. A setter on Object.prototype
// runs when the operation assigns its completion callback, and rewrites the
// path to an allowed one.
function startWithSwappedPath(denied, allowed, start) {
  const path = Buffer.from(denied);
  let swaps = 0;
  Object.defineProperty(Object.prototype, 'ondone', {
    __proto__: null,
    configurable: true,
    set(value) {
      swaps++;
      Buffer.from(allowed).copy(path);
      Object.defineProperty(this, 'ondone', {
        __proto__: null, value, writable: true, configurable: true,
      });
    },
  });
  try {
    return start(path);
  } finally {
    delete Object.prototype.ondone;
    assert.strictEqual(swaps, 1);
  }
}

function callbackResult(fn, path, ...args) {
  return new Promise((resolve, reject) => {
    fn(path, ...args, (err, result) => (err ? reject(err) : resolve(result)));
  });
}

const read = { code: 'ERR_ACCESS_DENIED', permission: 'FileSystemRead' };
const write = { code: 'ERR_ACCESS_DENIED', permission: 'FileSystemWrite' };
const data = Buffer.from('written');
const [allowedRead, deniedRead, allowedWrite, deniedWrite] = ${
  JSON.stringify([allowedRead, deniedRead, allowedWrite, deniedWrite])};

(async () => {
  await assert.rejects(startWithSwappedPath(
    deniedRead, allowedRead, (path) => callbackResult(fs.readFile, path)), read);
  await assert.rejects(startWithSwappedPath(
    deniedRead, allowedRead, (path) => fs.promises.readFile(path)), read);
  await assert.rejects(startWithSwappedPath(
    deniedWrite, allowedWrite, (path) => callbackResult(fs.writeFile, path, data)), write);
  await assert.rejects(startWithSwappedPath(
    deniedWrite, allowedWrite, (path) => fs.promises.writeFile(path, data)), write);
  // The allowed paths still work.
  assert.strictEqual(await fs.promises.readFile(allowedRead, 'utf8'), 'public');
  await fs.promises.writeFile(allowedWrite, data);
})().then(() => process.exitCode ??= 0);
`;

spawnSyncAndExitWithoutError(process.execPath, [
  '--permission',
  `--allow-fs-read=${allowedRead}`,
  `--allow-fs-write=${allowedWrite}`,
  '-e', script,
]);

assert.strictEqual(fs.existsSync(deniedWrite), false);
assert.strictEqual(fs.readFileSync(allowedWrite, 'utf8'), 'written');
