import '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';


{ // Verify unadulterated source is loaded when there are no loaders
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--no-warnings',
      fileURLToPath(fixtures.fileURL('es-modules', 'runmain.mjs')),
    ],
    { encoding: 'utf8' },
  );

  const resolveHookRunCount = [...(stdout.matchAll(/resolve passthru/g) ?? new Array())]
    .length - 1; // less 1 because the first is the needle

  assert.strictEqual(stderr, '');
  /**
   * resolveHookRunCount = 2:
   * 1. fixtures/…/runmain.mjs
   * 2. node:module (imported by fixtures/…/runmain.mjs)
   */
  assert.strictEqual(resolveHookRunCount, 2);
  assert.strictEqual(status, 0);
}
