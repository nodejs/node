// Test package map error handling: invalid JSON, missing fields, bad URLs, duplicates.
import '../common/index.mjs';
import assert from 'node:assert';
import { spawnSyncAndAssert, spawnSyncAndExit } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';
import tmpdir from '../common/tmpdir.js';
import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

tmpdir.refresh();

const cwd = fixtures.path('package-map/root');

// Throws ERR_PACKAGE_MAP_INVALID for invalid JSON.
spawnSyncAndExit(process.execPath, [
  '--no-warnings',
  '--experimental-package-map',
  fixtures.path('package-map/package-map-invalid-syntax.json'),
  '--input-type=module',
  '--eval', `import x from 'dep-a';`,
], { cwd }, {
  status: 1,
  signal: null,
  stderr: /ERR_PACKAGE_MAP_INVALID/,
});

// Throws ERR_PACKAGE_MAP_INVALID for missing packages field.
spawnSyncAndExit(process.execPath, [
  '--no-warnings',
  '--experimental-package-map',
  fixtures.path('package-map/package-map-invalid-schema.json'),
  '--input-type=module',
  '--eval', `import x from 'dep-a';`,
], { cwd }, {
  status: 1,
  signal: null,
  stderr(output) {
    assert.match(output, /ERR_PACKAGE_MAP_INVALID/);
    assert.match(output, /packages/);
  },
});

// Throws ERR_PACKAGE_MAP_KEY_NOT_FOUND for undefined dependency key.
spawnSyncAndExit(process.execPath, [
  '--no-warnings',
  '--experimental-package-map',
  fixtures.path('package-map/package-map-missing-dep.json'),
  '--input-type=module',
  '--eval', `import x from 'nonexistent';`,
], { cwd }, {
  status: 1,
  signal: null,
  stderr: /ERR_PACKAGE_MAP_KEY_NOT_FOUND/,
});

// Throws for non-existent map file.
spawnSyncAndExit(process.execPath, [
  '--no-warnings',
  '--experimental-package-map', '/nonexistent/package-map.json',
  '--input-type=module',
  '--eval', `import x from 'dep-a';`,
], { cwd }, {
  status: 1,
  signal: null,
  stderr(output) {
    assert.match(output, /ERR_PACKAGE_MAP_INVALID/);
    assert.match(output, /no such file or directory/);
  },
});

// Throws ERR_PACKAGE_MAP_INVALID for unsupported URL scheme in path.
spawnSyncAndExit(process.execPath, [
  '--no-warnings',
  '--experimental-package-map',
  fixtures.path('package-map/package-map-https-path.json'),
  '--input-type=module',
  '--eval', `import x from 'dep-a';`,
], { cwd }, {
  status: 1,
  signal: null,
  stderr(output) {
    assert.match(output, /ERR_PACKAGE_MAP_INVALID/);
    assert.match(output, /unsupported URL scheme/);
    assert.match(output, /https:\/\//);
  },
});

// Accepts file:// URLs in path.
{
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

  spawnSyncAndAssert(process.execPath, [
    '--no-warnings',
    '--experimental-package-map', fileUrlFixturePath,
    '--input-type=module',
    '--eval', `import dep from 'dep-a'; console.log(dep);`,
  ], { cwd }, { stdout: /dep-a-value/, trim: true });
}

// Throws ERR_PACKAGE_MAP_INVALID for duplicate package paths.
spawnSyncAndExit(process.execPath, [
  '--no-warnings',
  '--experimental-package-map',
  fixtures.path('package-map/package-map-duplicate-path.json'),
  '--input-type=module',
  '--eval', `import x from 'dep-a';`,
], { cwd }, {
  status: 1,
  signal: null,
  stderr(output) {
    assert.match(output, /ERR_PACKAGE_MAP_INVALID/);
    assert.match(output, /pkg-a/);
    assert.match(output, /pkg-b/);
  },
});
