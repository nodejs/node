'use strict';

require('../common');
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const filePath = tmpdir.resolve('temp.txt');
fs.writeFileSync(filePath, 'Test');

test('fs.stat should throw AbortError when abort signal is triggered', async () => {
  const signal = AbortSignal.abort();

  const { promise, resolve, reject } = Promise.withResolvers();
  fs.stat(filePath, { signal }, (err, stats) => {
    if (err) {
      return reject(err);
    }
    resolve(stats);
  });

  await assert.rejects(promise, { name: 'AbortError' });
  
  tmpdir.refresh();
});
