// This test confirms that `undefined`, `null`, and `[]`
// can be used as a placeholder for the second argument (`args`) of `spawn()`.
// Previously, there was a bug where using `undefined` for the second argument
// caused the third argument (`options`) to be ignored.
// See https://github.com/nodejs/node/issues/24912.

import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';

import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { once } from 'node:events';

tmpdir.refresh();

const command = common.isWindows ? 'cd' : 'pwd';
const options = { cwd: tmpdir.path };

if (common.isWindows) {
  // This test is not the case for Windows based systems
  // unless the `shell` options equals to `true`
  options.shell = true;
}

const testCases = [
  undefined,
  null,
  [],
];

const expectedResult = new Set([tmpdir.path.trim().toLowerCase()]);

const actualResults = new Set();

for (const testCase of testCases) {
  const subprocess = spawn(command, testCase, options);

  let accumulatedData = '';

  subprocess.stdout.setEncoding('utf8');
  subprocess.stdout.on('data', common.mustCall((data) => {
    accumulatedData += data;
  }));

  await once(subprocess.stdout, 'end');

  actualResults.add(accumulatedData.trim().toLowerCase());
}

assert.deepStrictEqual(actualResults, expectedResult);
