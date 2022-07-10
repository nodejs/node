import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';


spawn(execPath, [fixtures.path('/es-modules/cjs.js')])
  .then(mustCall(({ code, signal, stdout }) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'executed\n');
  }));
