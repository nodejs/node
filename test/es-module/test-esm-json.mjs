import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

import secret from '../fixtures/experimental.json' assert { type: 'json' };
import spawn from './helper.spawnAsPromised.mjs';


assert.strictEqual(secret.ofLife, 42);

// Test warning message
spawn(execPath, [
  fixtures.path('/es-modules/json-modules.mjs'),
])
  .then(mustCall(({ code, signal, stderr }) => {
    assert.ok(stderr.includes(
      'ExperimentalWarning: Importing JSON modules is an experimental feature. ' +
      'This feature could change at any time'
    ));
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
