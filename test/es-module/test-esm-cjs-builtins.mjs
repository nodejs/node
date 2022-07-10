import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';

const entry = fixtures.path('/es-modules/builtin-imports-case.mjs');

spawn(execPath, [entry])
  .then(mustCall(({ code, signal, stdout }) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'ok\n');
  }));
