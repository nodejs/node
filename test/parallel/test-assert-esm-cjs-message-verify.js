'use strict';

const { spawnPromisified } = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { writeFileSync, unlink } = require('fs');
const { describe, after, it } = require('node:test');

tmpdir.refresh();

const fileImports = {
  cjs: 'const assert = require("assert");',
  mjs: 'import assert from "assert";',
};

const fileNames = [];

for (const [ext, header] of Object.entries(fileImports)) {
  const fileName = `test-file.${ext}`;
  // Store the generated filesnames in an array
  fileNames.push(`${tmpdir.path}/${fileName}`);

  writeFileSync(tmpdir.resolve(fileName), `${header}\nassert.ok(0 === 2);`);
}

describe('ensure the assert.ok throwing similar error messages for esm and cjs files', () => {
  const nodejsPath = `${process.execPath}`;
  const errorsMessages = [];

  it('should return code 1 for each command', async () => {
    for (const fileName of fileNames) {
      const { stderr, code } = await spawnPromisified(nodejsPath, [fileName]);
      assert.strictEqual(code, 1);
      // For each error message, filter the lines which will starts with AssertionError
      errorsMessages.push(
        stderr.split('\n').find((s) => s.startsWith('AssertionError'))
      );
    }
  });

  after(() => {
    assert.strictEqual(errorsMessages.length, 2);
    assert.deepStrictEqual(errorsMessages[0], errorsMessages[1]);

    for (const fileName of fileNames) {
      unlink(fileName, () => {});
    }

    tmpdir.refresh();
  });
});
