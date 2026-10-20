// Test that package maps respect package.json conditional and pattern exports.
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';

const packageMapPath = fixtures.path('package-map/package-map.json');
const cwd = fixtures.path('package-map/root');

// Respects conditional exports (import).
spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map', packageMapPath,
  '--input-type=module',
  '--eval', `import { format } from 'dep-a'; console.log(format);`,
], { cwd }, { stdout: /esm/, trim: true });

// Respects pattern exports.
spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map', packageMapPath,
  '--input-type=module',
  '--eval', `import util from 'dep-a/lib/util'; console.log(util);`,
], { cwd }, { stdout: /dep-a-util/, trim: true });
