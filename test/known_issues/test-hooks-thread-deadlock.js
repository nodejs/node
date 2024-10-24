const { mustNotCall } = require('../common/index.js');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');
const { pathToFileURL } = require('node:url');

describe('hooks deadlock', { concurrency: true }, () => {
  it('will deadlock when a/hooks…resolve tries to contact the main thread during b/register', async () => {
    let stderr = '';
    let stdout = '';
    // ! Do NOT use spawnSync here: it will deadlock.
    const child = spawn(execPath, [
      '--import', fixtures.fileURL('es-module-loaders/inter-dependent-hooks/a/register.mjs')),
      '--import', fixtures.fileURL('es-module-loaders/inter-dependent-hooks/b/register.mjs')),
      '--input-type=module',
      '--eval',
      'import.meta.url;console.log("done")',
    ]);
    child.stderr.setEncoding('utf8');
    child.stderr.on('data', (data) => { stderr += data; });
    child.stdout.setEncoding('utf8');
    child.stdout.on('data', (data) => { stdout += data; });

    child.on('close', () => mustNotCall('Deadlock should prevent closing'));

    return new Promise((res, rej) => {
      setTimeout(() => {
        try {
          assert.strictEqual(stderr, '');
          assert.strictEqual(child.exitCode, null); // It hasn't ended
          assert.strictEqual(child.signalCode, null); // It hasn't ended

          assert.strictEqual(child.kill('SIGKILL'), true); // Deadlocked process must be forcibily killed.
          res('deadlocked process terminated');
        } catch (e) {
          rej(e);
        }
      }, 5_000); // This should have finished well before 5 second.
    });
  });
});
