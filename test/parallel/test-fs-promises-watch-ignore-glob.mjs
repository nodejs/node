import * as common from '../common/index.mjs';
import { skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();

const assert = await import('node:assert');
const path = await import('node:path');
const tmpdir = await import('../common/tmpdir.js');
const { setTimeout } = await import('node:timers/promises');
const { watch } = await import('node:fs/promises');
const { writeFileSync } = await import('node:fs');

tmpdir.refresh();

const testDir = tmpdir.resolve();
const keepFile = 'keep.txt';
const ignoreFile = 'ignore.log';
const keepFilePath = path.join(testDir, keepFile);
const ignoreFilePath = path.join(testDir, ignoreFile);

async function watchDir() {
  const watcher = watch(testDir, { ignore: '*.log' });

  for await (const { filename } of watcher) {
    assert.notStrictEqual(filename, ignoreFile);

    if (filename === keepFile) {
      break;
    }
  }
}

async function writeFiles() {
  if (common.isMacOS) {
    // Do the write with a delay to ensure that the OS is ready to notify us.
    // See https://github.com/nodejs/node/issues/52601.
    await setTimeout(common.platformTimeout(100));
  }

  writeFileSync(ignoreFilePath, 'ignored');
  writeFileSync(keepFilePath, 'content');
}

await Promise.all([watchDir(), writeFiles()]);
