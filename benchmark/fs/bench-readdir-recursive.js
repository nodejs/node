'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [10],
  dir: ['lib', 'test/parallel', 'test'],
  mode: ['sync', 'callback', 'promise'],
  withFileTypes: ['true', 'false'],
});

async function main({ n, dir, mode, withFileTypes }) {
  withFileTypes = withFileTypes === 'true';
  const fullPath = path.resolve(__dirname, '../../', dir);
  const options = { recursive: true, withFileTypes };
  let entries;

  bench.start();
  switch (mode) {
    case 'sync':
      for (let i = 0; i < n; i++) {
        entries = fs.readdirSync(fullPath, options);
      }
      break;
    case 'callback':
      for (let i = 0; i < n; i++) {
        entries = await new Promise((resolve, reject) => {
          fs.readdir(fullPath, options, (err, result) => {
            if (err) reject(err);
            else resolve(result);
          });
        });
      }
      break;
    case 'promise':
      for (let i = 0; i < n; i++) {
        entries = await fs.promises.readdir(fullPath, options);
      }
      break;
  }
  bench.end(n);

  assert.ok(entries.length > 0);
}
