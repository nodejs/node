import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';


// Expect note to be included in the error output
const expectedNote = 'To load an ES module, ' +
'set "type": "module" in the package.json ' +
'or use the .mjs extension.';

const mustIncludeMessage = {
  getMessage: () => (stderr) => `${expectedNote} not found in ${stderr}`,
  includeNote: true,
};
const mustNotIncludeMessage = {
  getMessage: () => (stderr) => `${expectedNote} must not be included in ${stderr}`,
  includeNote: false,
};

for (
  const { errorNeedle, filePath, getMessage, includeNote }
  of [
    {
      filePath: fixtures.path('/es-modules/es-note-unexpected-export-1.cjs'),
      ...mustIncludeMessage,
    },
    {
      filePath: fixtures.path('/es-modules/es-note-unexpected-import-1.cjs'),
      ...mustIncludeMessage,
    },
    {
      filePath: fixtures.path('/es-modules/es-note-promiserej-import-2.cjs'),
      ...mustNotIncludeMessage,
    },
    {
      filePath: fixtures.path('/es-modules/es-note-unexpected-import-3.cjs'),
      ...mustIncludeMessage,
    },
    {
      filePath: fixtures.path('/es-modules/es-note-unexpected-import-4.cjs'),
      ...mustIncludeMessage,
    },
    {
      filePath: fixtures.path('/es-modules/es-note-unexpected-import-5.cjs'),
      ...mustNotIncludeMessage,
    },
    {
      filePath: fixtures.path('/es-modules/es-note-error-1.mjs'),
      ...mustNotIncludeMessage,
      errorNeedle: 'Error: some error',
    },
    {
      filePath: fixtures.path('/es-modules/es-note-error-2.mjs'),
      ...mustNotIncludeMessage,
      errorNeedle: 'string',
    },
    {
      filePath: fixtures.path('/es-modules/es-note-error-3.mjs'),
      ...mustNotIncludeMessage,
      errorNeedle: 'null',
    },
    {
      filePath: fixtures.path('/es-modules/es-note-error-4.mjs'),
      ...mustNotIncludeMessage,
      errorNeedle: 'undefined',
    },
  ]
) {
  spawn(execPath, [filePath])
    .then(mustCall(({ code, stderr }) => {
      assert.strictEqual(code, 1);

      if (errorNeedle) stderr.includes(errorNeedle);

      const includesNote = stderr.includes(expectedNote);
      assert.ok(
        includeNote ? includesNote : !includesNote,
        `${filePath} ${getMessage(stderr)}`,
      );
    }));
}
