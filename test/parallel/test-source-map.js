'use strict';

if (!process.features.inspector) return;

const common = require('../common');
const assert = require('assert');
const { dirname } = require('path');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

let dirc = 0;
function nextdir() {
  return process.env.NODE_V8_COVERAGE ||
    path.join(tmpdir.path, `source_map_${++dirc}`);
}

// Outputs source maps when event loop is drained, with no async logic.
{
  const coverageDirectory = nextdir();
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/source-map/basic')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');
  const sourceMap = getSourceMapFromCache('basic.js', coverageDirectory);
  assert.strictEqual(sourceMap.url, 'https://http.cat/418');
}

// Outputs source maps when process.kill(process.pid, "SIGINT"); exits process.
{
  const coverageDirectory = nextdir();
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/source-map/sigint')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  if (!common.isWindows) {
    if (output.signal !== 'SIGINT') {
      console.log(output.stderr.toString());
    }
    assert.strictEqual(output.signal, 'SIGINT');
  }
  assert.strictEqual(output.stderr.toString(), '');
  const sourceMap = getSourceMapFromCache('sigint.js', coverageDirectory);
  assert.strictEqual(sourceMap.url, 'https://http.cat/402');
}

// Outputs source maps when source-file calls process.exit(1).
{
  const coverageDirectory = nextdir();
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/source-map/exit-1')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.stderr.toString(), '');
  const sourceMap = getSourceMapFromCache('exit-1.js', coverageDirectory);
  assert.strictEqual(sourceMap.url, 'https://http.cat/404');
}

// Outputs source-maps for esm module.
{
  const coverageDirectory = nextdir();
  const output = spawnSync(process.execPath, [
    '--no-warnings',
    '--experimental-modules',
    require.resolve('../fixtures/source-map/esm-basic.mjs')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.stderr.toString(), '');
  const sourceMap = getSourceMapFromCache('esm-basic.mjs', coverageDirectory);
  assert.strictEqual(sourceMap.url, 'https://http.cat/405');
}

// Loads source-maps with relative path from .map file on disk.
{
  const coverageDirectory = nextdir();
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/source-map/disk-relative-path')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');
  const sourceMap = getSourceMapFromCache(
    'disk-relative-path.js',
    coverageDirectory
  );
  // Source-map should have been loaded from disk and sources should have been
  // rewritten, such that they're absolute paths.
  assert.strictEqual(
    dirname(
      `file://${require.resolve('../fixtures/source-map/disk-relative-path')}`),
    dirname(sourceMap.data.sources[0])
  );
}

// Loads source-maps from inline data URL.
{
  const coverageDirectory = nextdir();
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/source-map/inline-base64.js')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');
  const sourceMap = getSourceMapFromCache(
    'inline-base64.js',
    coverageDirectory
  );
  // base64 JSON should have been decoded, and paths to sources should have
  // been rewritten such that they're absolute:
  assert.strictEqual(
    dirname(
      `file://${require.resolve('../fixtures/source-map/inline-base64')}`),
    dirname(sourceMap.data.sources[0])
  );
}

function getSourceMapFromCache(fixtureFile, coverageDirectory) {
  const jsonFiles = fs.readdirSync(coverageDirectory);
  for (const jsonFile of jsonFiles) {
    const maybeSourceMapCache = require(
      path.join(coverageDirectory, jsonFile)
    )['source-map-cache'] || {};
    const keys = Object.keys(maybeSourceMapCache);
    for (const key of keys) {
      if (key.includes(fixtureFile)) {
        return maybeSourceMapCache[key];
      }
    }
  }
}
