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
    'url',
  ],
});

async function load(n, fixtureFileURL) {
  const array = [];
  for (let i = 0; i < n; i++) {
    array[i] = await import(`${fixtureFileURL}?i=${i}`);
  }
  return array;
}

function main({ n, valuesToRead }) {
  const fixtureDir = path.resolve(__filename, '../../fixtures');
  const fixtureFile = path.join(fixtureDir, `import-meta-${valuesToRead}.mjs`);
  const fixtureFileURL = pathToFileURL(fixtureFile);

  load(n, fixtureFileURL).then((array) => {
    const results = new Array(n);
    bench.start();
    for (let i = 0; i < n; i++) {
      results[i] = array[i].default();
    }
    bench.end(n);

    switch (valuesToRead) {
      case 'dirname-and-filename':
        assert.deepStrictEqual(results[n - 1], [fixtureDir, fixtureFile]);
        break;
      case 'dirname':
        assert.strictEqual(results[n - 1], fixtureDir);
        break;
      case 'filename':
        assert.strictEqual(results[n - 1], fixtureFile);
        break;
      case 'url':
        assert.strictEqual(results[n - 1], `${fixtureFileURL}?i=${n - 1}`);
        break;
    }
  });
}
