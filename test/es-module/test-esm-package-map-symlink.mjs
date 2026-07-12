// Test that package maps resolve correctly through a symlinked ancestor directory.
import { canCreateSymLink } from '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';
import tmpdir from '../common/tmpdir.js';
import { symlinkSync } from 'node:fs';
import path from 'node:path';

if (!canCreateSymLink()) {
  process.exit(0);
}

tmpdir.refresh();

const symlinkDir = tmpdir.resolve('symlinked-package-map-esm');
symlinkSync(fixtures.path('package-map'), symlinkDir, 'dir');

spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map', path.join(symlinkDir, 'package-map.json'),
  '--input-type=module',
  '--eval', `import dep from 'dep-a'; console.log(dep);`,
], { cwd: fixtures.path('package-map/root') }, {
  stdout: /dep-a-value/,
  trim: true,
});
