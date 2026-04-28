import { canCreateSymLink, spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { symlinkSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';
import { pathToFileURL } from 'node:url';

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

describe('ESM: --experimental-package-map', () => {

  // =========== Basic Resolution ===========

  describe('basic resolution', () => {
    it('resolves a direct dependency', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a'; console.log(dep);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-value/);
      assert.strictEqual(code, 0, stderr);
    });

    it('resolves a subpath export', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import util from 'dep-a/lib/util'; console.log(util);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-util/);
      assert.strictEqual(code, 0, stderr);
    });

    it('resolves transitive dependency from allowed package', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        // dep-b can access dep-a
        `import depB from 'dep-b'; console.log(depB);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-b using dep-a-value/);
      assert.strictEqual(code, 0, stderr);
    });
  });

  // =========== Fallback Behavior ===========

  describe('resolution boundaries', () => {
    it('falls back for builtin modules', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import fs from 'node:fs'; console.log(typeof fs.readFileSync);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /function/);
      assert.strictEqual(code, 0, stderr);
    });

    it('throws when parent not in map', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a'; console.log(dep);`,
      ], { cwd: tmpdir.path });  // Not in any mapped package

      assert.match(stderr, /ERR_PACKAGE_MAP_EXTERNAL_FILE/);
      assert.notStrictEqual(code, 0, stderr);
    });
  });

  // =========== Error Handling ===========

  describe('error handling', () => {
    it('throws ERR_PACKAGE_MAP_INVALID for invalid JSON', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-invalid-syntax.json'),
        '--input-type=module',
        '--eval', `import x from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.notStrictEqual(code, 0, stderr);
    });

    it('throws ERR_PACKAGE_MAP_INVALID for missing packages field', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-invalid-schema.json'),
        '--input-type=module',
        '--eval', `import x from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.match(stderr, /packages/);
      assert.notStrictEqual(code, 0, stderr);
    });

    it('throws ERR_PACKAGE_MAP_KEY_NOT_FOUND for undefined dependency key', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-missing-dep.json'),
        '--input-type=module',
        '--eval',
        `import x from 'nonexistent';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.match(stderr, /ERR_PACKAGE_MAP_KEY_NOT_FOUND/);
      assert.notStrictEqual(code, 0, stderr);
    });

    it('throws for non-existent map file', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', '/nonexistent/package-map.json',
        '--input-type=module',
        '--eval', `import x from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.match(stderr, /no such file or directory/);
      assert.notStrictEqual(code, 0, stderr);
    });

    it('throws ERR_PACKAGE_MAP_INVALID for unsupported URL scheme in path', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-https-path.json'),
        '--input-type=module',
        '--eval', `import x from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.match(stderr, /unsupported URL scheme/);
      assert.match(stderr, /https:\/\//);
      assert.notStrictEqual(code, 0, stderr);
    });

    it('accepts file:// URLs in path', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', fileUrlFixturePath,
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a'; console.log(dep);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-value/);
      assert.strictEqual(code, 0, stderr);
    });

    it('throws ERR_PACKAGE_MAP_INVALID for duplicate package paths', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-duplicate-path.json'),
        '--input-type=module',
        '--eval', `import x from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.match(stderr, /pkg-a/);
      assert.match(stderr, /pkg-b/);
      assert.notStrictEqual(code, 0, stderr);
    });
  });

  // =========== Zero Overhead When Disabled ===========

  describe('disabled by default', () => {
    it('has no impact when flag not set', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--input-type=module',
        '--eval',
        `import fs from 'node:fs'; console.log('ok');`,
      ]);

      assert.strictEqual(stderr, '');
      assert.match(stdout, /ok/);
      assert.strictEqual(code, 0, stderr);
    });
  });

  // =========== Exports Integration ===========

  describe('package.json exports integration', () => {
    it('respects conditional exports (import)', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import { format } from 'dep-a'; console.log(format);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /esm/);  // Should get ESM export
      assert.strictEqual(code, 0, stderr);
    });

    it('respects pattern exports', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import util from 'dep-a/lib/util'; console.log(util);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-util/);
      assert.strictEqual(code, 0, stderr);
    });
  });

  // =========== Experimental Warning ===========

  describe('experimental warning', () => {
    it('emits experimental warning on first use', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.match(stderr, /ExperimentalWarning/);
      assert.match(stderr, /Package map/i);
      assert.strictEqual(code, 0, stderr);
    });
  });

  // =========== Path Containment ===========

  describe('path containment edge cases', () => {
    it('does not match pkg-other as being inside pkg', async () => {
      // Regression test: pkg-other (at ./pkg-other) should not be
      // incorrectly matched as inside pkg (at ./pkg) just because
      // the string "./pkg-other" starts with "./pkg"
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/package-map-path-prefix.json'),
        '--input-type=module',
        '--eval',
        `import pkg from 'pkg'; console.log(pkg);`,
      ], { cwd: fixtures.path('package-map/pkg-other') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /pkg-value/);
      assert.strictEqual(code, 0, stderr);
    });
  });

  // =========== External Package Paths ===========

  describe('external package paths', () => {
    it('resolves packages outside the package map directory via relative paths', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map',
        fixtures.path('package-map/nested-project/package-map-external-deps.json'),
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a'; console.log(dep);`,
      ], { cwd: fixtures.path('package-map/nested-project/src') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-value/);
      assert.strictEqual(code, 0, stderr);
    });
  });

  // =========== Same Request, Different Versions ===========

  describe('same request resolves to different versions', () => {
    const versionForkMap = fixtures.path('package-map/package-map-version-fork.json');

    it('resolves the same bare specifier to different packages depending on the importer', async () => {
      // app-18 and app-19 both import 'react', but the package map wires
      // them to react@18 and react@19 respectively.
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', versionForkMap,
        '--input-type=module',
        '--eval',
        `import app18 from 'app-18'; import app19 from 'app-19'; console.log(app18); console.log(app19);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /app-18 using react 18/);
      assert.match(stdout, /app-19 using react 19/);
      assert.strictEqual(code, 0, stderr);
    });
  });

  // =========== Longest Path Wins ===========

  describe('longest path wins', () => {
    const longestPathMap = fixtures.path('package-map/package-map-longest-path.json');

    it('resolves nested package using its own dependencies, not the parent', async () => {
      // Inner lives at ./root/node_modules/inner which is inside root's
      // path (./root). The longest matching path should win, so code in
      // inner should resolve dep-a (inner's dep), not be treated as root.
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', longestPathMap,
        '--input-type=module',
        '--eval',
        `import inner from 'inner'; console.log(inner);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /inner using dep-a-value/);
      assert.strictEqual(code, 0, stderr);
    });

    it('falls back for undeclared dependency in nested package parent', async () => {
      // Root does not list dep-a in its dependencies, so importing it
      // from root should fail with ERR_MODULE_NOT_FOUND.
      const { code, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', longestPathMap,
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
      assert.notStrictEqual(code, 0, stderr);
    });
  });

  // =========== Symlink in Ancestor ===========

  describe('symlink in ancestor of package map', {
    skip: !canCreateSymLink() && 'insufficient privileges for symlinks',
  }, () => {
    const symlinkDir = tmpdir.resolve('symlinked-package-map-esm');
    symlinkSync(fixtures.path('package-map'), symlinkDir, 'dir');

    it('resolves through a symlinked ancestor directory', async () => {
      const symlinkMapPath = path.join(symlinkDir, 'package-map.json');
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-package-map', symlinkMapPath,
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a'; console.log(dep);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(stderr, '');
      assert.match(stdout, /dep-a-value/);
      assert.strictEqual(code, 0, stderr);
    });
  });
});
