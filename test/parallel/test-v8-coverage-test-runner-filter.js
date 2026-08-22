'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { spawnSync } = require('child_process');

common.skipIfInspectorDisabled();

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const sourceMap = Buffer.from(JSON.stringify({
  version: 3,
  sources: ['original.js'],
  mappings: '',
})).toString('base64');
const mapComment = `//# sourceMappingURL=data:application/json;base64,${sourceMap}`;

const appPath = tmpdir.resolve('app.js');
const depPath = tmpdir.resolve(path.join('node_modules', 'dep', 'index.js'));
fs.mkdirSync(path.dirname(depPath), { recursive: true });
// Source maps of files inside node_modules are only cached when node_modules
// source map support is enabled explicitly.
fs.writeFileSync(appPath, 'require(\'module\')' +
  '.setSourceMapsSupport(true, { nodeModules: true });\n' +
  `require('dep');\n${mapComment}\n`);
fs.writeFileSync(depPath, `module.exports = 42;\n${mapComment}\n`);

const appUrl = pathToFileURL(appPath).href;
const depUrl = pathToFileURL(depPath).href;

let dirc = 0;
// `makeEnv` receives the coverage directory and returns extra env vars.
function runWithCoverage(makeEnv) {
  const coverageDirectory = tmpdir.resolve(`cov_${++dirc}`);
  const output = spawnSync(process.execPath, [appPath], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
      NODE_V8_COVERAGE: coverageDirectory,
      ...makeEnv(coverageDirectory),
    },
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');

  const urls = [];
  const sourceMapCacheKeys = [];
  for (const coverageFile of fs.readdirSync(coverageDirectory)) {
    const coverage =
      JSON.parse(fs.readFileSync(path.join(coverageDirectory, coverageFile)));
    for (const script of coverage.result) {
      urls.push(script.url);
    }
    sourceMapCacheKeys.push(...Object.keys(coverage['source-map-cache'] ?? {}));
  }
  return { coverageDirectory, urls, sourceMapCacheKeys };
}

// Filter directory and node_modules exclusion set: only file: URLs outside of
// node_modules remain, in the coverage results as well as in the source map
// cache.
{
  const { urls, sourceMapCacheKeys } = runWithCoverage((dir) => ({
    NODE_TEST_COVERAGE_FILTER_DIR: dir,
    NODE_TEST_COVERAGE_EXCLUDE_NODE_MODULES: '1',
  }));
  assert.ok(urls.length > 0);
  assert.ok(urls.every((url) => url.startsWith('file:')), urls.join(','));
  assert.ok(urls.includes(appUrl));
  assert.ok(!urls.includes(depUrl));
  assert.ok(urls.every((url) => !url.includes('/node_modules/')));
  assert.deepStrictEqual(sourceMapCacheKeys, [appUrl]);
}

// Filter directory alone: node:* internals are dropped, node_modules are
// kept.
{
  const { urls, sourceMapCacheKeys } = runWithCoverage((dir) => ({
    NODE_TEST_COVERAGE_FILTER_DIR: dir,
  }));
  assert.ok(urls.every((url) => url.startsWith('file:')), urls.join(','));
  assert.ok(urls.includes(appUrl));
  assert.ok(urls.includes(depUrl));
  assert.ok(sourceMapCacheKeys.includes(appUrl));
  assert.ok(sourceMapCacheKeys.includes(depUrl));
}

// A filter directory that does not match the coverage directory must have no
// effect: this is what keeps user-facing NODE_V8_COVERAGE output complete
// when the env var leaks into a process that redirects NODE_V8_COVERAGE.
{
  const { urls } = runWithCoverage(() => ({
    NODE_TEST_COVERAGE_FILTER_DIR: tmpdir.resolve('some_other_dir'),
    NODE_TEST_COVERAGE_EXCLUDE_NODE_MODULES: '1',
  }));
  assert.ok(urls.some((url) => url.startsWith('node:')));
  assert.ok(urls.includes(appUrl));
  assert.ok(urls.includes(depUrl));
}

// Plain NODE_V8_COVERAGE keeps the full profile, including node:* internals,
// with the source map cache appended.
{
  const { urls, sourceMapCacheKeys } = runWithCoverage(() => ({}));
  assert.ok(urls.some((url) => url.startsWith('node:')));
  assert.ok(urls.includes(appUrl));
  assert.ok(urls.includes(depUrl));
  assert.ok(sourceMapCacheKeys.includes(appUrl));
  assert.ok(sourceMapCacheKeys.includes(depUrl));
}
