'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// The permission model denies access before the first await, so the stack
// still has the frames of a caller that does not await the promise.
const { status, stderr } = spawnSync(
  process.execPath,
  [
    '--permission',
    '-e',
    `
      const assert = require('assert');
      const { readFile, writeFile, appendFile } = require('fs/promises');
      const target = ${JSON.stringify(tmpdir.resolve('file'))};

      function readCaller() { return readFile(target); }
      function writeCaller() { return writeFile(target, 'data'); }
      function appendCaller() { return appendFile(target, 'data'); }
      async function awaitAppendCaller() { await appendFile(target, 'data'); }

      const cases = [
        [readCaller, /at readCaller /],
        [writeCaller, /at writeCaller /],
        [appendCaller, /at appendCaller /],
        [awaitAppendCaller, /at awaitAppendCaller /],
      ];
      let checked = 0;
      for (const [caller, stack] of cases) {
        assert.rejects(caller(), { code: 'ERR_ACCESS_DENIED', stack })
          .then(() => checked++);
      }
      process.on('exit', () => assert.strictEqual(checked, cases.length));
    `,
  ],
);
assert.strictEqual(status, 0, stderr.toString());
