import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';

const importStatement = 'import { foo, notfound } from \'./module-named-exports.mjs\';';
const importStatementMultiline = `import {
  foo,
  notfound
} from './module-named-exports.mjs';
`;

for (
  const input
  of [
    importStatement,
    importStatementMultiline,
  ]
) {
  spawn(execPath, [
    '--input-type=module',
    '--eval',
    input,
  ], {
    cwd: fixtures.path('es-module-loaders'),
  })
    .then(mustCall(({ code, stderr, stdout }) => {
      assert.notStrictEqual(code, 0);

      // SyntaxError: The requested module './module-named-exports.mjs'
      // does not provide an export named 'notfound'
      assert.match(stderr, /SyntaxError:/);
      // The quotes ensure that the path starts with ./ and not ../
      assert.match(stderr, /'\.\/module-named-exports\.mjs'/);
      assert.match(stderr, /notfound/);
    }));
}
