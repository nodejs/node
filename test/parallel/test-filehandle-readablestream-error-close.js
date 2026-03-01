'use strict';

const common = require('../common');
const assert = require('assert');

async function runScript(source) {
  const child = await common.spawnPromisified(process.execPath, [
    '--unhandled-rejections=strict',
    '-e',
    source,
  ]);

  assert.strictEqual(child.code, 0, child.stderr);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stderr, '');
}

// Regression: once a readableWebStream read fails, explicitly closing the
// FileHandle must not trigger an unhandled rejection.
(async () => {
  await runScript(`
    const { closeSync } = require('node:fs');
    const { open } = require('node:fs/promises');
    const assert = require('node:assert');

    async function consume(readable) {
      for await (const _ of readable);
    }

    (async () => {
      const file = await open(process.execPath);
      const readable = file.readableWebStream();
      closeSync(file.fd);

      await assert.rejects(consume(readable), { code: 'EBADF' });
      await assert.rejects(file.close(), { code: 'EBADF' });
    })().catch((err) => {
      console.error(err);
      process.exitCode = 1;
    });
  `);
})().then(common.mustCall());

// Edge: BYOB readers should not leak unhandled rejections on the same path.
(async () => {
  await runScript(`
    const { closeSync } = require('node:fs');
    const { open } = require('node:fs/promises');
    const assert = require('node:assert');

    (async () => {
      const file = await open(process.execPath);
      const readable = file.readableWebStream();
      const reader = readable.getReader({ mode: 'byob' });
      closeSync(file.fd);

      await assert.rejects(reader.read(new DataView(new ArrayBuffer(1024))), {
        code: 'EBADF',
      });
      await assert.rejects(file.close(), { code: 'EBADF' });
    })().catch((err) => {
      console.error(err);
      process.exitCode = 1;
    });
  `);
})().then(common.mustCall());

// Safety: successful reads must remain unaffected.
(async () => {
  await runScript(`
    const { open } = require('node:fs/promises');

    (async () => {
      const file = await open(process.execPath);
      for await (const _ of file.readableWebStream());
      await file.close();
    })().catch((err) => {
      console.error(err);
      process.exitCode = 1;
    });
  `);
})().then(common.mustCall());
