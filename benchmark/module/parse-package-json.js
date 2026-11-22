'use strict';

// Parses the package.json files found under deps/ with the internal
// parsePackageJSON() binding, grouped by file size.

const common = require('../common.js');
const fs = require('fs');
const path = require('path');

const buckets = {
  '0-512': [0, 512],
  '512-1024': [512, 1024],
  '1024-2048': [1024, 2048],
  '2048+': [2048, Infinity],
};

const bench = common.createBenchmark(main, {
  size: Object.keys(buckets),
  n: [1e6],
}, {
  flags: ['--expose-internals'],
});

function loadCorpus(parsePackageJSON) {
  const deps = path.resolve(__dirname, '../../deps');
  const seen = new Set();
  const corpus = [];
  for (const entry of fs.readdirSync(deps, { recursive: true })) {
    if (path.basename(entry) !== 'package.json') continue;
    const source = fs.readFileSync(path.join(deps, entry));
    const key = source.toString('latin1');
    if (seen.has(key)) continue;
    seen.add(key);
    try {
      parsePackageJSON(source, '/benchmark/package.json');
    } catch {
      continue;  // Skip test fixtures that are not valid package configs.
    }
    corpus.push(source);
  }
  return corpus;
}

function main({ size, n }) {
  const { internalBinding } = require('internal/test/binding');
  const { parsePackageJSON } = internalBinding('modules');
  const [min, max] = buckets[size];
  const files = loadCorpus(parsePackageJSON)
    .filter((source) => source.length >= min && source.length < max);
  if (files.length === 0) {
    throw new Error(`no package.json under deps/ in the ${size} bucket`);
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    parsePackageJSON(files[i % files.length], '/benchmark/package.json');
  }
  bench.end(n);
}
