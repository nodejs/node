'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  size: ['minimal', 'typical', 'large'],
  n: [1e5],
}, { flags: ['--expose-internals', '--no-warnings'] });

function makeMinimal() {
  return { main: 'index.js' };
}

function makeTypical() {
  return {
    name: '@scope/typical-package',
    version: '3.14.15',
    description: 'A package with "escapes", unicode é中 and a\ttab',
    type: 'module',
    main: './dist/index.cjs',
    exports: {
      '.': {
        types: './dist/index.d.ts',
        import: './dist/index.mjs',
        require: './dist/index.cjs',
      },
      './utils': { import: './dist/utils.mjs', require: './dist/utils.cjs' },
      './package.json': './package.json',
    },
    imports: { '#internal/*': './src/internal/*.js' },
    scripts: { build: 'tsc -p .', test: 'node --test', lint: 'eslint .' },
    keywords: ['json', 'parser', 'benchmark'],
    private: false,
    dependencies: { 'left-pad': '^1.3.0', 'lodash': '^4.17.21' },
  };
}

function makeLarge() {
  const pkg = makeTypical();
  pkg.dependencies = {};
  pkg.devDependencies = {};
  for (let i = 0; i < 250; i++) {
    pkg.dependencies[`dependency-number-${i}`] = `^${i % 17}.${i % 7}.${i}`;
    pkg.devDependencies[`@types/dev-dependency-${i}`] = `~${i}.0.0`;
  }
  for (let i = 0; i < 100; i++) {
    pkg.exports[`./feature-${i}`] = {
      types: `./dist/feature-${i}.d.ts`,
      import: `./dist/feature-${i}.mjs`,
      require: `./dist/feature-${i}.cjs`,
    };
  }
  pkg.contributors = Array.from({ length: 50 }, (_, i) => ({
    name: `Contributor ${i}`,
    email: `contributor${i}@example.com`,
  }));
  return pkg;
}

const payloads = {
  minimal: makeMinimal,
  typical: makeTypical,
  large: makeLarge,
};

function main({ size, n }) {
  const { internalBinding } = require('internal/test/binding');
  const { parsePackageJSON } = internalBinding('modules');
  const source = Buffer.from(JSON.stringify(payloads[size](), null, 2));
  const jsonPath = `/benchmark/${size}/package.json`;

  const expected = parsePackageJSON(source, jsonPath);
  assert.ok(Array.isArray(expected));

  let result;
  bench.start();
  for (let i = 0; i < n; i++) {
    result = parsePackageJSON(source, jsonPath);
  }
  bench.end(n);
  assert.deepStrictEqual(result, expected);
}
