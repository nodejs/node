'use strict';

if (!process.features.inspector) return;

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

let dirc = 0;
function nextdir() {
  return `cov_${++dirc}`;
}

// Outputs coverage when event loop is drained, with no async logic.
{
  const coverageDirectory = path.join(tmpdir.path, nextdir());
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/v8-coverage/basic')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');
  const fixtureCoverage = getFixtureCoverage('basic.js', coverageDirectory);
  assert.ok(fixtureCoverage);
  // first branch executed.
  assert.strictEqual(fixtureCoverage.functions[1].ranges[0].count, 1);
  // second branch did not execute.
  assert.strictEqual(fixtureCoverage.functions[1].ranges[1].count, 0);
}

// Outputs coverage when process.exit(1) exits process.
{
  const coverageDirectory = path.join(tmpdir.path, nextdir());
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/v8-coverage/exit-1')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.status, 1);
  assert.strictEqual(output.stderr.toString(), '');
  const fixtureCoverage = getFixtureCoverage('exit-1.js', coverageDirectory);
  assert.ok(fixtureCoverage, 'coverage not found for file');
  // first branch executed.
  assert.strictEqual(fixtureCoverage.functions[1].ranges[0].count, 1);
  // second branch did not execute.
  assert.strictEqual(fixtureCoverage.functions[1].ranges[1].count, 0);
}

// Outputs coverage when process.kill(process.pid, "SIGINT"); exits process.
{
  const coverageDirectory = path.join(tmpdir.path, nextdir());
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/v8-coverage/sigint')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  if (!common.isWindows) {
    assert.strictEqual(output.signal, 'SIGINT');
  }
  assert.strictEqual(output.stderr.toString(), '');
  const fixtureCoverage = getFixtureCoverage('sigint.js', coverageDirectory);
  assert.ok(fixtureCoverage);
  // first branch executed.
  assert.strictEqual(fixtureCoverage.functions[1].ranges[0].count, 1);
  // second branch did not execute.
  assert.strictEqual(fixtureCoverage.functions[1].ranges[1].count, 0);
}

// outputs coverage from subprocess.
{
  const coverageDirectory = path.join(tmpdir.path, nextdir());
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/v8-coverage/spawn-subprocess')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');
  const fixtureCoverage = getFixtureCoverage('subprocess.js',
                                             coverageDirectory);
  assert.ok(fixtureCoverage);
  // first branch executed.
  assert.strictEqual(fixtureCoverage.functions[2].ranges[0].count, 1);
  // second branch did not execute.
  assert.strictEqual(fixtureCoverage.functions[2].ranges[1].count, 0);
}

// outputs coverage from worker.
{
  const coverageDirectory = path.join(tmpdir.path, nextdir());
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/v8-coverage/worker')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');
  const fixtureCoverage = getFixtureCoverage('subprocess.js',
                                             coverageDirectory);
  assert.ok(fixtureCoverage);
  // first branch executed.
  assert.strictEqual(fixtureCoverage.functions[2].ranges[0].count, 1);
  // second branch did not execute.
  assert.strictEqual(fixtureCoverage.functions[2].ranges[1].count, 0);
}

// Does not output coverage if NODE_V8_COVERAGE is empty.
{
  const coverageDirectory = path.join(tmpdir.path, nextdir());
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/v8-coverage/spawn-subprocess-no-cov')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');
  const fixtureCoverage = getFixtureCoverage('subprocess.js',
                                             coverageDirectory);
  assert.strictEqual(fixtureCoverage, undefined);
}

// disables async hooks before writing coverage.
{
  const coverageDirectory = path.join(tmpdir.path, nextdir());
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/v8-coverage/async-hooks')
  ], { env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory } });
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');
  const fixtureCoverage = getFixtureCoverage('async-hooks.js',
                                             coverageDirectory);
  assert.ok(fixtureCoverage);
  // first branch executed.
  assert.strictEqual(fixtureCoverage.functions[1].ranges[0].count, 1);
}

// Outputs coverage when the coverage directory is not absolute.
{
  const coverageDirectory = nextdir();
  const absoluteCoverageDirectory = path.join(tmpdir.path, coverageDirectory);
  const output = spawnSync(process.execPath, [
    require.resolve('../fixtures/v8-coverage/basic')
  ], {
    cwd: tmpdir.path,
    env: { ...process.env, NODE_V8_COVERAGE: coverageDirectory }
  });
  assert.strictEqual(output.status, 0);
  assert.strictEqual(output.stderr.toString(), '');
  const fixtureCoverage = getFixtureCoverage('basic.js',
                                             absoluteCoverageDirectory);
  assert.ok(fixtureCoverage);
  // first branch executed.
  assert.strictEqual(fixtureCoverage.functions[1].ranges[0].count, 1);
  // second branch did not execute.
  assert.strictEqual(fixtureCoverage.functions[1].ranges[1].count, 0);
}

// Extracts the coverage object for a given fixture name.
function getFixtureCoverage(fixtureFile, coverageDirectory) {
  const coverageFiles = fs.readdirSync(coverageDirectory);
  for (const coverageFile of coverageFiles) {
    const coverage = require(path.join(coverageDirectory, coverageFile));
    for (const fixtureCoverage of coverage.result) {
      if (fixtureCoverage.url.indexOf(`/${fixtureFile}`) !== -1) {
        return fixtureCoverage;
      }
    }
  }
}
