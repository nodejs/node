import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

const packageMapPath = fixtures.path('package-map/package-map.json');

describe('ESM: --experimental-package-map', () => {

  // =========== Basic Resolution ===========

  describe('basic resolution', () => {
    it('resolves a direct dependency', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a'; console.log(dep);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /dep-a-value/);
    });

    it('resolves a subpath export', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import util from 'dep-a/lib/util'; console.log(util);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /dep-a-util/);
    });

    it('resolves transitive dependency from allowed package', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        // dep-b can access dep-a
        `import depB from 'dep-b'; console.log(depB);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /dep-b using dep-a-value/);
    });
  });

  // =========== Access Control ===========

  describe('dependency access control', () => {
    it('throws ERR_PACKAGE_MAP_ACCESS_DENIED for undeclared dependency', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import x from 'not-a-dep';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.notStrictEqual(code, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_ACCESS_DENIED/);
      assert.match(stderr, /not-a-dep/);
      assert.match(stderr, /root/);  // parent package name
    });

    it('includes package key in error for nameless packages', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/package-map-nameless.json'),
        '--input-type=module',
        '--eval',
        `import x from 'forbidden';`,
      ], { cwd: fixtures.path('package-map/nameless-pkg') });

      assert.notStrictEqual(code, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_ACCESS_DENIED/);
      // Should show key since package is nameless
      assert.match(stderr, /nameless/);
    });
  });

  // =========== Fallback Behavior ===========

  describe('resolution boundaries', () => {
    it('falls back for builtin modules', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import fs from 'node:fs'; console.log(typeof fs.readFileSync);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /function/);
    });

    it('throws when parent not in map', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a'; console.log(dep);`,
      ], { cwd: '/tmp' });  // Not in any mapped package

      assert.notStrictEqual(code, 0);
      assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
    });
  });

  // =========== Error Handling ===========

  describe('error handling', () => {
    it('throws ERR_PACKAGE_MAP_INVALID for invalid JSON', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/package-map-invalid-syntax.json'),
        '--input-type=module',
        '--eval', `import x from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.notStrictEqual(code, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
    });

    it('throws ERR_PACKAGE_MAP_INVALID for missing packages field', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/package-map-invalid-schema.json'),
        '--input-type=module',
        '--eval', `import x from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.notStrictEqual(code, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.match(stderr, /packages/);
    });

    it('throws ERR_PACKAGE_MAP_KEY_NOT_FOUND for undefined dependency key', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/package-map-missing-dep.json'),
        '--input-type=module',
        '--eval',
        `import x from 'nonexistent';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.notStrictEqual(code, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_KEY_NOT_FOUND/);
    });

    it('throws for non-existent map file', async () => {
      const { code, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', '/nonexistent/package-map.json',
        '--input-type=module',
        '--eval', `import x from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.notStrictEqual(code, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_INVALID/);
      assert.match(stderr, /not found/);
    });
  });

  // =========== Zero Overhead When Disabled ===========

  describe('disabled by default', () => {
    it('has no impact when flag not set', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--input-type=module',
        '--eval',
        `import fs from 'node:fs'; console.log('ok');`,
      ]);

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /ok/);
    });
  });

  // =========== Exports Integration ===========

  describe('package.json exports integration', () => {
    it('respects conditional exports (import)', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import { format } from 'dep-a'; console.log(format);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /esm/);  // Should get ESM export
    });

    it('respects pattern exports', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', packageMapPath,
        '--input-type=module',
        '--eval',
        `import util from 'dep-a/lib/util'; console.log(util);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /dep-a-util/);
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

      assert.strictEqual(code, 0);
      assert.match(stderr, /ExperimentalWarning/);
      assert.match(stderr, /Package map/i);
    });
  });

  // =========== Path Containment ===========

  describe('path containment edge cases', () => {
    it('does not match pkg-other as being inside pkg', async () => {
      // Regression test: pkg-other (at ./pkg-other) should not be
      // incorrectly matched as inside pkg (at ./pkg) just because
      // the string "./pkg-other" starts with "./pkg"
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/package-map-path-prefix.json'),
        '--input-type=module',
        '--eval',
        `import pkg from 'pkg'; console.log(pkg);`,
      ], { cwd: fixtures.path('package-map/pkg-other') });

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /pkg-value/);
    });
  });

  // =========== External Package Paths ===========

  describe('external package paths', () => {
    it('resolves packages outside the package map directory via relative paths', async () => {
      const { code, stdout, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map',
        fixtures.path('package-map/nested-project/package-map-external-deps.json'),
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a'; console.log(dep);`,
      ], { cwd: fixtures.path('package-map/nested-project/src') });

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /dep-a-value/);
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
        '--experimental-package-map', longestPathMap,
        '--input-type=module',
        '--eval',
        `import inner from 'inner'; console.log(inner);`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.strictEqual(code, 0, stderr);
      assert.match(stdout, /inner using dep-a-value/);
    });

    it('denies access to nested package deps from parent package', async () => {
      // Root does not list dep-a in its dependencies, so importing it
      // from root should fail even though inner (nested inside root) can.
      const { code, stderr } = await spawnPromisified(execPath, [
        '--experimental-package-map', longestPathMap,
        '--input-type=module',
        '--eval',
        `import dep from 'dep-a';`,
      ], { cwd: fixtures.path('package-map/root') });

      assert.notStrictEqual(code, 0);
      assert.match(stderr, /ERR_PACKAGE_MAP_ACCESS_DENIED/);
    });
  });
});
