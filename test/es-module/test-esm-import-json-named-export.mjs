import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';


spawn(execPath, [
  fixtures.path('es-modules', 'import-json-named-export.mjs'),
])
  .then(mustCall(({ code, stderr }) => {
    // SyntaxError: The requested module '../experimental.json'
    // does not provide an export named 'ofLife'
    assert.match(stderr, /SyntaxError:/);
    assert.match(stderr, /'\.\.\/experimental\.json'/);
    assert.match(stderr, /'ofLife'/);

    assert.notStrictEqual(code, 0);
  }));
