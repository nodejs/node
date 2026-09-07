'use strict';

// Loading modules that carry source maps, with source map support enabled.
// This is the cost paid at startup by applications bundled or transpiled with
// source maps; the maps are only consulted if a stack trace is generated.

const fs = require('fs');
const path = require('path');
const common = require('../common.js');
const tmpdir = require('../../test/common/tmpdir');
const benchmarkDirectory = tmpdir.resolve('nodejs-benchmark-module-source-map');

const bench = common.createBenchmark(main, {
  sourceMap: ['none', 'inline', 'external'],
  n: [1000],
}, {
  setup(configs) {
    tmpdir.refresh();
    const maxN = configs.reduce((max, c) => Math.max(max, c.n), 0);
    createModules(maxN);
  },
});

function moduleSource(i) {
  const methods = [];
  for (let m = 0; m < 40; m++) {
    methods.push(`  method${m}(input) { return [].concat(input).map((item) => ({ item, m: ${m}, service: ${i} })); }`);
  }
  return `'use strict';
class Service${i} {
  constructor(options = {}) { this.options = { retries: 3, ...options }; }
${methods.join('\n')}
}
function helper${i}(list) { return list.filter(Boolean).slice(0, ${i % 7}); }
module.exports = { Service${i}, helper${i} };
`;
}

function sourceMapFor(i, source) {
  return JSON.stringify({
    version: 3,
    file: `${i}.js`,
    sources: [`../src/${i}.ts`],
    sourcesContent: [source],
    names: [],
    mappings: 'AAAA;' + 'AACA,MAAM;'.repeat(44),
  });
}

function createModules(n) {
  for (const kind of ['none', 'inline', 'external']) {
    const dir = path.join(benchmarkDirectory, kind);
    fs.mkdirSync(dir, { recursive: true });
    for (let i = 0; i < n; i++) {
      const source = moduleSource(i);
      let trailer = '';
      if (kind === 'inline') {
        const data = Buffer.from(sourceMapFor(i, source)).toString('base64');
        trailer = `//# sourceMappingURL=data:application/json;base64,${data}\n`;
      } else if (kind === 'external') {
        fs.writeFileSync(path.join(dir, `${i}.js.map`), sourceMapFor(i, source));
        trailer = `//# sourceMappingURL=${i}.js.map\n`;
      }
      fs.writeFileSync(path.join(dir, `${i}.js`), source + trailer);
    }
  }
}

function main({ sourceMap, n }) {
  process.setSourceMapsEnabled(true);
  const dir = path.join(benchmarkDirectory, sourceMap);
  bench.start();
  for (let i = 0; i < n; i++) {
    require(path.join(dir, `${i}.js`));
  }
  bench.end(n);
}
