'use strict';

const common = require('../common');
const assert = require('assert');
const { Blob, Buffer } = require('buffer');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const filename = 'open-as-blob-sync.txt';
const testfile = tmpdir.resolve(filename);
const mutationFile = tmpdir.resolve('open-as-blob-sync-mutation.txt');
const missing = tmpdir.resolve('does-not-exist.txt');
const data = 'hello openAsBlobSync';

fs.writeFileSync(testfile, data);
fs.writeFileSync(mutationFile, data);

assert.throws(() => fs.openAsBlobSync(1), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => fs.openAsBlobSync(testfile, null), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => fs.openAsBlobSync(testfile, { type: 1 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => fs.openAsBlobSync(missing), {
  code: 'ENOENT',
  syscall: 'stat',
  path: missing,
});

(async () => {
  for (const path of [
    testfile,
    Buffer.from(testfile),
    tmpdir.fileURL(filename),
  ]) {
    const blob = fs.openAsBlobSync(path, { type: 'text/plain' });

    assert.ok(blob instanceof Blob);
    assert.strictEqual(blob.size, Buffer.byteLength(data));
    assert.strictEqual(blob.type, 'text/plain');
    assert.strictEqual(await blob.text(), data);
  }

  const promise = fs.openAsBlob(testfile);
  assert.ok(promise instanceof Promise);
  await promise;

  const blob = fs.openAsBlobSync(mutationFile);
  fs.writeFileSync(mutationFile, `${data}!`);
  await assert.rejects(blob.text(), {
    name: 'NotReadableError',
  });
})().then(common.mustCall());
