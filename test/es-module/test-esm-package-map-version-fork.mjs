// Test that the same bare specifier resolves to different packages depending on the importer.
import '../common/index.mjs';
import assert from 'node:assert';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';

spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--experimental-package-map',
  fixtures.path('package-map/package-map-version-fork.json'),
  '--input-type=module',
  '--eval',
  `import app18 from 'app-18'; import app19 from 'app-19'; console.log(app18); console.log(app19);`,
], { cwd: fixtures.path('package-map/root') }, {
  stdout(output) {
    assert.match(output, /app-18 using react 18/);
    assert.match(output, /app-19 using react 19/);
  },
});
