import '../common/index.mjs';
import { test } from 'node:test';
import * as fixtures from '../common/fixtures.mjs';
import { spawnSync } from 'node:child_process';
import assert from 'node:assert';

test('correctly reports error for a longer stack trace', () => {
  // The following regex matches the error message that is expected to be thrown
  //
  // package-type-module/require-esm-error-annotation/index.cjs:6
  //   require('./app.js')
  //   ^

  const fixture = fixtures.path('es-modules/package-type-module/require-esm-error-annotation/index.cjs');
  const args = ['--no-experimental-require-module', fixture];
  const regex = /index\.cjs:6[\n\r]+\s{2}require\('\.\/app\.js'\)[\n\r]+\s{2}\^/;

  const result = spawnSync(process.execPath, args);
  const stderr = result.stderr.toString();

  assert.strictEqual(result.status, 1);
  assert.strictEqual(result.stdout.toString(), '');
  assert.match(stderr, regex);
});
