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
      '--no-warnings',
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

    assert.strictEqual(stderr, '');
    assert.match(stdout, /dep-a-value/);
    assert.strictEqual(status, 0, stderr);
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

    assert.match(stderr, /ExperimentalWarning/);
    assert.match(stderr, /[Pp]ackage map/);
    assert.strictEqual(status, 0, stderr);
  });

  it('accepts relative path', () => {
    const { status, stdout, stderr } = spawnSync(process.execPath, [
      '--no-warnings',
      '--experimental-package-map', '../package-map.json',
      '-e',
      `const dep = require('dep-a'); console.log(dep.default);`,
    ], {
      cwd: fixtures.path('package-map/root'),
      encoding: 'utf8',
    });

    assert.strictEqual(stderr, '');
    // Relative path ../package-map.json resolved from cwd (root/)
    assert.match(stdout, /dep-a-value/);
    assert.strictEqual(status, 0, stderr);
  });

  it('accepts absolute path', () => {
    const { status, stderr } = spawnSync(process.execPath, [
      '--no-warnings',
      '--experimental-package-map', fixtures.path('package-map/package-map.json'),
      '-e',
      `console.log('ok');`,
    ], {
      encoding: 'utf8',
    });

    assert.strictEqual(stderr, '');
    assert.strictEqual(status, 0, stderr);
  });
});
