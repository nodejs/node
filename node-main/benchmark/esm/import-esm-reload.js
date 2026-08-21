'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
const assert = require('assert');
const { pathToFileURL } = require('url');
const bench = common.createBenchmark(main, {
  count: [1, 100],
  n: [1000],
});

function prepare(count) {
  tmpdir.refresh();
  const dir = tmpdir.resolve('modules');
  fs.mkdirSync(dir, { recursive: true });
  let modSource = '';
  for (let i = 0; i < count; ++i) {
    modSource += `export const value${i} = 1;\n`;
  }
  const script = tmpdir.resolve('mod.js');
  fs.writeFileSync(script, modSource, 'utf8');
  return script;
}

async function main({ n, count }) {
  const script = prepare(count);
  const url = pathToFileURL(script).href;
  let result = 0;
  bench.start();
  for (let i = 0; i < n; i++) {
    const mod = await import(`${url}?t=${i}`);
    result += mod[`value${count - 1}`];
  }
  bench.end(n);
  assert.strictEqual(result, n);
}
