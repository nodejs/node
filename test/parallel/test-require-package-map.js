'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { describe, it } = require('node:test');

const packageMapPath = fixtures.path('package-map/package-map.json');

describe('CJS: --experimental-package-map', () => {

  describe('basic resolution', () => {
    it('resolves require() through package map', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map', packageMapPath,
        '-e',
        `const dep = require('dep-a'); console.log(dep.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(status, 0, stderr);
      assert.match(stdout, /dep-a-value/);
    });

    it('resolves subpath require()', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map', packageMapPath,
        '-e',
        `const util = require('dep-a/lib/util'); console.log(util.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(status, 0, stderr);
      assert.match(stdout, /dep-a-util/);
    });

    it('resolves transitive dependencies', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map', packageMapPath,
        '-e',
        `const depB = require('dep-b'); console.log(depB.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(status, 0, stderr);
      assert.match(stdout, /dep-b using dep-a-value/);
    });
  });

  describe('dependency access control', () => {
    it('throws for undeclared dependency', () => {
      const { status, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map', packageMapPath,
        '-e',
        `require('not-a-dep');`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.notStrictEqual(status, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_ACCESS_DENIED/);
    });
  });

  describe('fallback behavior', () => {
    it('falls back for builtin modules', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map', packageMapPath,
        '-e',
        `const fs = require('fs'); console.log(typeof fs.readFileSync);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(status, 0, stderr);
      assert.match(stdout, /function/);
    });

    it('falls back when parent not in map', () => {
      const { status, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map', packageMapPath,
        '-e',
        `require('dep-a');`,
      ], {
        cwd: '/tmp',
        encoding: 'utf8',
      });

      // Should fall back to standard resolution (which will fail)
      assert.notStrictEqual(status, 0);
      assert.match(stderr, /Cannot find module/);
    });
  });

  describe('error handling', () => {
    it('throws for invalid JSON', () => {
      const { status, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/package-map-invalid-syntax.json'),
        '-e',
        `require('dep-a');`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.notStrictEqual(status, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
    });

    it('throws for missing packages field', () => {
      const { status, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/package-map-invalid-schema.json'),
        '-e',
        `require('dep-a');`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.notStrictEqual(status, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
    });
  });

  describe('conditional exports', () => {
    it('respects require condition in exports', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map', packageMapPath,
        '-e',
        `const dep = require('dep-a'); console.log(dep.format);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(status, 0, stderr);
      assert.match(stdout, /cjs/);  // Should get CJS export
    });
  });

  describe('disabled by default', () => {
    it('has no impact when flag not set', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '-e',
        `const fs = require('fs'); console.log('ok');`,
      ], {
        encoding: 'utf8',
      });

      assert.strictEqual(status, 0, stderr);
      assert.match(stdout, /ok/);
    });
  });

  describe('path containment edge cases', () => {
    it('does not match pkg-other as being inside pkg', () => {
      // Regression test: pkg-other (at ./pkg-other) should not be
      // incorrectly matched as inside pkg (at ./pkg) just because
      // the string "./pkg-other" starts with "./pkg"
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/package-map-path-prefix.json'),
        '-e',
        `const pkg = require('pkg'); console.log(pkg.default);`,
      ], {
        cwd: fixtures.path('package-map/pkg-other'),
        encoding: 'utf8',
      });

      assert.strictEqual(status, 0, stderr);
      assert.match(stdout, /pkg-value/);
    });
  });
});
