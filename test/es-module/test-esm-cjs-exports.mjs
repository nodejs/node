import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';


const validEntry = fixtures.path('/es-modules/cjs-exports.mjs');
spawn(execPath, [validEntry])
  .then(mustCall(({ code, signal, stdout }) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'ok\n');
  }));

const invalidEntry = fixtures.path('/es-modules/cjs-exports-invalid.mjs');
spawn(execPath, [invalidEntry])
  .then(mustCall(({ code, signal, stderr }) => {
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.ok(stderr.includes('Warning: To load an ES module'));
    assert.ok(stderr.includes('Unexpected token \'export\''));
  }));
