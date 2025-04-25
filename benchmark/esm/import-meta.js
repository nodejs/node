'use strict';

const path = require('path');
const { pathToFileURL } = require('url');
const common = require('../common');
const assert = require('assert');
const bench = common.createBenchmark(main, {
  n: [1000],
  valuesToRead: [
    'dirname-and-filename',
    'dirname',
    'filename',
  ],
});

const fixtureDir = path.resolve(__filename, '../../fixtures');
const fixtureDirURL = pathToFileURL(fixtureDir);
async function load(array, n, valuesToRead) {
  for (let i = 0; i < n; i++) {
    array[i] = await import(`${fixtureDirURL}/import-meta-${valuesToRead}.mjs?i=${i}`);
  }
  return array;
}

function main({ n, valuesToRead }) {
  const array = [];
  for (let i = 0; i < n; ++i) {
    array.push({ dirname: '', filename: '', i: 0 });
  }

  bench.start();
  load(array, n, valuesToRead).then((arr) => {
    bench.end(n);
    if (valuesToRead.includes('dirname')) assert.strictEqual(arr[n - 1].dirname, fixtureDir);
    if (valuesToRead.includes('filename')) assert.strictEqual(arr[n - 1].filename, path.join(fixtureDir, `import-meta-${valuesToRead}.mjs`));
  });
}
