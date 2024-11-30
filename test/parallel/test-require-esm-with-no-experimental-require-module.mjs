import '../common/index.mjs';
import { test } from 'node:test';
import * as fixtures from '../common/fixtures.mjs';
import { spawnSync } from 'node:child_process';
import assert from 'node:assert';
import { EOL } from 'node:os';

test('correctly reports error for a longer stack trace', () => {
  // The following regex matches the error message that is expected to be thrown
  //
  // package-type-module/require-esm-error-annotation/longer-stack.cjs:6
  //   require('./app.js')
  //   ^

  const fixture = fixtures.path('es-modules/package-type-module/require-esm-error-annotation/index.cjs');
  const args = ['--no-experimental-require-module', fixture];

  const lineNumber = 6;
  const lineContent = "require('./app.js')";
  const fullMessage = `${fixture}:${lineNumber}${EOL}  ${lineContent}${EOL}  ^${EOL}`;

  const result = spawnSync(process.execPath, args);

  console.log(`STDERR: ${result.stderr.toString()}`);

  assert.strictEqual(result.status, 1);
  assert.strictEqual(result.stdout.toString(), '');
  assert(result.stderr.toString().indexOf(fullMessage) > -1);
});
