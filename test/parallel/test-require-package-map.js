'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { symlinkSync, writeFileSync } = require('node:fs');
const path = require('node:path');
const { describe, it } = require('node:test');
const { pathToFileURL } = require('node:url');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const packageMapPath = fixtures.path('package-map/package-map.json');

// Generated at runtime because file:// URLs must be absolute, making them machine-dependent.
const fileUrlFixturePath = tmpdir.resolve('package-map-file-url.json');
writeFileSync(fileUrlFixturePath, JSON.stringify({
  packages: {
    'root': {
      path: pathToFileURL(fixtures.path('package-map/root')).href,
      dependencies: { 'dep-a': 'dep-a' },
    },
    'dep-a': {
      path: pathToFileURL(fixtures.path('package-map/dep-a')).href,
      dependencies: {},
    },
  },
}));

describe('CJS: --experimental-package-map', { concurrency: !process.env.TEST_PARALLEL }, () => {

  describe('basic resolution', () => {
    it('resolves require() through package map', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '-e',
        `const dep = require('dep-a'); console.log(dep.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-value/);
      assert.strictEqual(status, 0, stderr);
    });

    it('resolves subpath require()', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '-e',
        `const util = require('dep-a/lib/util'); console.log(util.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-util/);
      assert.strictEqual(status, 0, stderr);
    });

    it('resolves transitive dependencies', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '-e',
        `const depB = require('dep-b'); console.log(depB.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-b using dep-a-value/);
      assert.strictEqual(status, 0, stderr);
    });
  });

  describe('resolution boundaries', () => {
    it('falls back for builtin modules', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '-e',
        `const fs = require('fs'); console.log(typeof fs.readFileSync);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /function/);
      assert.strictEqual(status, 0, stderr);
    });

    it('throws when parent not in map', () => {
      const { status, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '-e',
        `require('dep-a');`,
      ], {
        cwd: tmpdir.path,
        encoding: 'utf8',
      });

      assert.match(stderr, /ERR_PACKAGE_MAP_EXTERNAL_FILE/);
      assert.notStrictEqual(status, 0, stderr);
    });
  });

  describe('error handling', () => {
    it('throws for invalid JSON', () => {
      const { status, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-invalid-syntax.json'),
        '-e',
        `require('dep-a');`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.notStrictEqual(status, 0, stderr);
    });

    it('throws for missing packages field', () => {
      const { status, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-invalid-schema.json'),
        '-e',
        `require('dep-a');`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.notStrictEqual(status, 0, stderr);
    });

    it('throws for unsupported URL scheme in path', () => {
      const { status, stderr } = spawnSync(process.execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/package-map-https-path.json'),
        '-e',
        `require('dep-a');`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.match(stderr, /unsupported URL scheme/);
      assert.match(stderr, /https:\/\//);
      assert.notStrictEqual(status, 0, stderr);
    });

    it('accepts file:// URLs in path', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', fileUrlFixturePath,
        '-e',
        `const dep = require('dep-a'); console.log(dep.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-value/);
      assert.strictEqual(status, 0, stderr);
    });

    it('throws for duplicate package paths', () => {
      const { status, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-duplicate-path.json'),
        '-e',
        `require('dep-a');`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.match(stderr, /pkg-a/);
      assert.match(stderr, /pkg-b/);
      assert.notStrictEqual(status, 0, stderr);
    });
  });

  describe('conditional exports', () => {
    it('respects require condition in exports', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '-e',
        `const dep = require('dep-a'); console.log(dep.format);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /cjs/);  // Should get CJS export
      assert.strictEqual(status, 0, stderr);
    });
  });

  describe('disabled by default', () => {
    it('has no impact when flag not set', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '-e',
        `const fs = require('fs'); console.log('ok');`,
      ], {
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /ok/);
      assert.strictEqual(status, 0, stderr);
    });
  });

  describe('path containment edge cases', () => {
    it('does not match pkg-other as being inside pkg', () => {
      // Regression test: pkg-other (at ./pkg-other) should not be
      // incorrectly matched as inside pkg (at ./pkg) just because
      // the string "./pkg-other" starts with "./pkg"
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-path-prefix.json'),
        '-e',
        `const pkg = require('pkg'); console.log(pkg.default);`,
      ], {
        cwd: fixtures.path('package-map/pkg-other'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /pkg-value/);
      assert.strictEqual(status, 0, stderr);
    });
  });

  describe('external package paths', () => {
    it('resolves packages outside the package map directory via relative paths', () => {
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/nested-project/package-map-external-deps.json'),
        '-e',
        `const dep = require('dep-a'); console.log(dep.default);`,
      ], {
        cwd: fixtures.path('package-map/nested-project/src'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-value/);
      assert.strictEqual(status, 0, stderr);
    });
  });

  describe('same request resolves to different versions', () => {
    const versionForkMap = fixtures.path('package-map/package-map-version-fork.json');

    it('resolves the same bare specifier to different packages depending on the importer', () => {
      // app-18 and app-19 both require 'react', but the package map wires
      // them to react@18 and react@19 respectively.
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', versionForkMap,
        '-e',
        `const app18 = require('app-18'); const app19 = require('app-19'); console.log(app18.default); console.log(app19.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /app-18 using react 18/);
      assert.match(stdout, /app-19 using react 19/);
      assert.strictEqual(status, 0, stderr);
    });
  });

  describe('longest path wins', () => {
    const longestPathMap = fixtures.path('package-map/package-map-longest-path.json');

    it('resolves nested package using its own dependencies, not the parent', () => {
      // Inner lives at ./root/node_modules/inner which is inside root's
      // path (./root). The longest matching path should win, so code in
      // inner should resolve dep-a (inner's dep), not be treated as root.
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', longestPathMap,
        '-e',
        `const inner = require('inner'); console.log(inner.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /inner using dep-a-value/);
      assert.strictEqual(status, 0, stderr);
    });

    it('falls back for undeclared dependency in nested package parent', () => {
      // Root does not list dep-a in its dependencies, so requiring it
      // from root should fail with MODULE_NOT_FOUND.
      const { status, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', longestPathMap,
        '-e',
        `require('dep-a');`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.match(stderr, /Cannot find module/);
      assert.notStrictEqual(status, 0);
    });
  });

  describe('symlink in ancestor of package map', {
    skip: !common.canCreateSymLink() && 'insufficient privileges for symlinks',
  }, () => {
    const symlinkDir = tmpdir.resolve('symlinked-package-map-cjs');
    symlinkSync(fixtures.path('package-map'), symlinkDir, 'dir');

    it('resolves through a symlinked ancestor directory', () => {
      const symlinkMapPath = path.join(symlinkDir, 'package-map.json');
      const { status, stdout, stderr } = spawnSync(process.execPath, [
        '--no-warnings',
        '--experimental-package-map', symlinkMapPath,
        '-e',
        `const dep = require('dep-a'); console.log(dep.default);`,
      ], {
        cwd: fixtures.path('package-map/root'),
        encoding: 'utf8',
      });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-value/);
      assert.strictEqual(status, 0, stderr);
    });
  });
});
