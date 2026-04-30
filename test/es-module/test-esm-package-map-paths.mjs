// Test package map path handling: containment, external paths, longest-path-wins.
import '../common/index.mjs';
import { spawnSyncAndAssert, spawnSyncAndExit } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';

// Path containment: does not match pkg-other as being inside pkg.
spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map',
  fixtures.path('package-map/package-map-path-prefix.json'),
  '--input-type=module',
  '--eval', `import pkg from 'pkg'; console.log(pkg);`,
], { cwd: fixtures.path('package-map/pkg-other') }, {
  stdout: /pkg-value/,
  trim: true,
});

// External paths: resolves packages outside the package map directory.
spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map',
  fixtures.path('package-map/nested-project/package-map-external-deps.json'),
  '--input-type=module',
  '--eval', `import dep from 'dep-a'; console.log(dep);`,
], { cwd: fixtures.path('package-map/nested-project/src') }, {
  stdout: /dep-a-value/,
  trim: true,
});

// Longest path wins: nested package uses its own dependencies, not the parent's.
{
  const longestPathMap = fixtures.path('package-map/package-map-longest-path.json');
  const cwd = fixtures.path('package-map/root');

  spawnSyncAndAssert(process.execPath, [
    '--no-warnings',
    '--experimental-package-map', longestPathMap,
    '--input-type=module',
    '--eval', `import inner from 'inner'; console.log(inner);`,
  ], { cwd }, { stdout: /inner using dep-a-value/, trim: true });

  // Root does not list dep-a, so importing it from root should fail.
  spawnSyncAndExit(process.execPath, [
    '--no-warnings',
    '--experimental-package-map', longestPathMap,
    '--input-type=module',
    '--eval', `import dep from 'dep-a';`,
  ], { cwd }, {
    status: 1,
    signal: null,
    stderr: /ERR_MODULE_NOT_FOUND/,
  });
}
