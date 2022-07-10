import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';


spawn(execPath, [fixtures.path('es-module-url/native.mjs')])
  .then(mustCall(({ code }) => {
    assert.strictEqual(code, 1);
  }));
