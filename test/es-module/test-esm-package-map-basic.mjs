// Test that package maps resolve direct, subpath, and transitive dependencies.
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';

const packageMapPath = fixtures.path('package-map/package-map.json');
const cwd = fixtures.path('package-map/root');

// Resolves a direct dependency.
spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map', packageMapPath,
  '--input-type=module',
  '--eval', `import dep from 'dep-a'; console.log(dep);`,
], { cwd }, { stdout: /dep-a-value/, trim: true });

// Resolves a subpath export.
spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map', packageMapPath,
  '--input-type=module',
  '--eval', `import util from 'dep-a/lib/util'; console.log(util);`,
], { cwd }, { stdout: /dep-a-util/, trim: true });

// Resolves transitive dependency from allowed package.
spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map', packageMapPath,
  '--input-type=module',
  '--eval', `import depB from 'dep-b'; console.log(depB);`,
], { cwd }, { stdout: /dep-b using dep-a-value/, trim: true });
