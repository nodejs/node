import * as common from '../common/index.mjs';
import { skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();

// if (common.isSunOS)
//   common.skip('`fs.watch()` is not reliable on SunOS.');

const assert = await import('node:assert');
const path = await import('node:path');
const tmpdir = await import('../common/tmpdir.js');
const { watch } = await import('node:fs/promises');
const { writeFileSync } = await import('node:fs');

tmpdir.refresh();

const testDir = tmpdir.resolve();
const keepFile = 'keep.txt';
const ignoreLog = 'debug.log';
const ignoreTmp = 'temp.tmp';
const keepFilePath = path.join(testDir, keepFile);
const ignoreLogPath = path.join(testDir, ignoreLog);
const ignoreTmpPath = path.join(testDir, ignoreTmp);

const watcher = watch(testDir, {
  ignore: [
    '*.log',
    /\.tmp$/,
  ],
});

// Do the write with a delay to ensure that the OS is ready to notify us. See
// https://github.com/nodejs/node/issues/52601.
setTimeout(() => {
  writeFileSync(ignoreLogPath, 'ignored');
  writeFileSync(ignoreTmpPath, 'ignored');
  writeFileSync(keepFilePath, 'content');
}, common.platformTimeout(100));

for await (const { filename } of watcher) {
  assert.notStrictEqual(filename, ignoreLog);
  assert.notStrictEqual(filename, ignoreTmp);

  if (filename === keepFile) {
    break;
  }
}
