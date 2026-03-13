'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { describe, it } = require('node:test');

const onlyIfNodeOptionsSupport = {
  skip: process.config.variables.node_without_node_options,
};

describe('--experimental-package-map CLI behavior', () => {

  it('works via NODE_OPTIONS', onlyIfNodeOptionsSupport, () => {
    const { status, stdout, stderr } = spawnSync(process.execPath, [
      '-e',
      `const dep = require('dep-a'); console.log(dep);`,
    ], {
      cwd: fixtures.path('package-map/root'),
      encoding: 'utf8',
      env: {
        ...process.env,
        NODE_OPTIONS: `--experimental-package-map=${JSON.stringify(fixtures.path('package-map/package-map.json'))}`,
      },
    });

    assert.strictEqual(status, 0, stderr);
    assert.match(stdout, /dep-a-value/);
  });

  it('emits experimental warning on first use', () => {
    const { status, stderr } = spawnSync(process.execPath, [
      '--experimental-package-map', fixtures.path('package-map/package-map.json'),
      '-e',
      `require('dep-a');`,
    ], {
      cwd: fixtures.path('package-map/root'),
      encoding: 'utf8',
    });

    assert.strictEqual(status, 0);
    assert.match(stderr, /ExperimentalWarning/);
    assert.match(stderr, /[Pp]ackage map/);
  });

  it('accepts relative path', () => {
    const { status, stdout, stderr } = spawnSync(process.execPath, [
      '--experimental-package-map', '../package-map.json',
      '-e',
      `const dep = require('dep-a'); console.log(dep.default);`,
    ], {
      cwd: fixtures.path('package-map/root'),
      encoding: 'utf8',
    });

    // Relative path ../package-map.json resolved from cwd (root/)
    assert.strictEqual(status, 0, stderr);
    assert.match(stdout, /dep-a-value/);
  });

  it('accepts absolute path', () => {
    const { status, stderr } = spawnSync(process.execPath, [
      '--experimental-package-map', fixtures.path('package-map/package-map.json'),
      '-e',
      `console.log('ok');`,
    ], {
      encoding: 'utf8',
    });

    assert.strictEqual(status, 0, stderr);
  });

  it('does not emit warning when flag not set', () => {
    const { status, stderr } = spawnSync(process.execPath, [
      '-e',
      `console.log('ok');`,
    ], {
      encoding: 'utf8',
    });

    assert.strictEqual(status, 0);
    assert.doesNotMatch(stderr, /ExperimentalWarning/);
    assert.doesNotMatch(stderr, /[Pp]ackage map/);
  });

  it('can be combined with other flags', () => {
    const { status, stdout, stderr } = spawnSync(process.execPath, [
      '--experimental-package-map', fixtures.path('package-map/package-map.json'),
      '--no-warnings',
      '-e',
      `const dep = require('dep-a'); console.log(dep);`,
    ], {
      cwd: fixtures.path('package-map/root'),
      encoding: 'utf8',
    });

    assert.strictEqual(status, 0, stderr);
    assert.match(stdout, /dep-a-value/);
    // Warning should be suppressed
    assert.doesNotMatch(stderr, /ExperimentalWarning/);
  });
});
