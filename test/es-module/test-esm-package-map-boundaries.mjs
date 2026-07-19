// Test that package maps fall back for builtins and reject files outside mapped packages.
import '../common/index.mjs';
import { spawnSyncAndAssert, spawnSyncAndExit } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const packageMapPath = fixtures.path('package-map/package-map.json');
const cwd = fixtures.path('package-map/root');

// Falls back for builtin modules.
spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map', packageMapPath,
  '--input-type=module',
  '--eval', `import fs from 'node:fs'; console.log(typeof fs.readFileSync);`,
], { cwd }, { stdout: /function/, trim: true });

// Throws when parent not in map.
spawnSyncAndExit(process.execPath, [
  '--no-warnings',
  '--experimental-package-map', packageMapPath,
  '--input-type=module',
  '--eval', `import dep from 'dep-a'; console.log(dep);`,
], { cwd: tmpdir.path }, {
  status: 1,
  signal: null,
  stderr: /ERR_PACKAGE_MAP_EXTERNAL_FILE/,
});
