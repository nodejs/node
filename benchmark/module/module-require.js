'use strict';

const fs = require('fs');
const common = require('../common.js');
const tmpdir = require('../../test/common/tmpdir');
const benchmarkDirectory = tmpdir.resolve('nodejs-benchmark-module');

const t = 1e4;

const bench = common.createBenchmark(main, {
  type: ['.js', '.json', 'dir'],
  n: [t],
}, {
  setup() {
    tmpdir.refresh();
    createEntryPoint(t);
  },
  teardown() {
    tmpdir.refresh();
  },
});

function main({ type, n }) {
  switch (type) {
    case '.js':
      measureJSFile(n);
      break;
    case '.json':
      measureJSONFile(n);
      break;
    case 'dir':
      measureDir(n);
  }
}

function measureJSFile(n) {
  bench.start();
  for (let i = 0; i < n; i++) {
    require(`${benchmarkDirectory}/${i}`);
  }
  bench.end(n);
}

function measureJSONFile(n) {
  bench.start();
  for (let i = 0; i < n; i++) {
    require(`${benchmarkDirectory}/json_${i}.json`);
  }
  bench.end(n);
}

function measureDir(n) {
  bench.start();
  for (let i = 0; i < n; i++) {
    require(`${benchmarkDirectory}${i}`);
  }
  bench.end(n);
}

function createEntryPoint(n) {
  fs.mkdirSync(benchmarkDirectory);

  const JSFileContent = 'module.exports = [];';
  const JSONFileContent = '[]';

  for (let i = 0; i < n; i++) {
    // JS file.
    fs.writeFileSync(`${benchmarkDirectory}/${i}.js`, JSFileContent);

    // JSON file.
    fs.writeFileSync(`${benchmarkDirectory}/json_${i}.json`, JSONFileContent);

    // Dir.
    fs.mkdirSync(`${benchmarkDirectory}${i}`);
    fs.writeFileSync(
      `${benchmarkDirectory}${i}/package.json`,
      '{"main": "index.js"}',
    );
    fs.writeFileSync(
      `${benchmarkDirectory}${i}/index.js`,
      JSFileContent,
    );
  }
}
