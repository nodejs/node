import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';


{ // Verify unadulterated source is loaded when there are no loaders
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--no-warnings',
      fixtures.path('es-modules', 'runmain.mjs'),
    ],
    { encoding: 'utf8' },
  );

  // Length minus 1 because the first match is the needle.
  const resolveHookRunCount = (stdout.match(/resolve passthru/g)?.length ?? 0) - 1;

  assert.strictEqual(stderr, '');
  /**
   * resolveHookRunCount = 2:
   * 1. fixtures/…/runmain.mjs
   * 2. node:module (imported by fixtures/…/runmain.mjs)
   */
  assert.strictEqual(resolveHookRunCount, 2);
  assert.strictEqual(status, 0);
}
