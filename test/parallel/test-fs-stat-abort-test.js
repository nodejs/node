'use strict';

require('../common');
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const tmpdir = require('../common/tmpdir');

test('fs.stat should throw AbortError when called with an already aborted AbortSignal', async () => {
  // This test verifies that fs.stat immediately throws an AbortError if the provided AbortSignal
  // has already been canceled. This approach is used because trying to abort an fs.stat call in flight
  // is unreliable given that file system operations tend to complete very quickly on many platforms.
  tmpdir.refresh();

  const filePath = tmpdir.resolve('temp.txt');
  fs.writeFileSync(filePath, 'Test');

  // Create an already aborted AbortSignal.
  const signal = AbortSignal.abort();

  const { promise, resolve, reject } = Promise.withResolvers();
  fs.stat(filePath, { signal }, (err, stats) => {
    if (err) {
      return reject(err);
    }
    resolve(stats);
  });

  // Assert that the promise is rejected with an AbortError.
  await assert.rejects(promise, { name: 'AbortError' });

  fs.unlinkSync(filePath);
  tmpdir.refresh();
});
