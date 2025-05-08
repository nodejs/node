import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.js';
import assert from 'node:assert';

common.spawnPromisified(process.execPath, [
  '--test',
  fixtures.path('test-runner', 'force-color.js'),
], {
  env: {
    ...process.env,
    FORCE_COLOR: 3,
  }
}).then(common.mustCall((result) => {
  assert.strictEqual(result.code, 0);
}));
