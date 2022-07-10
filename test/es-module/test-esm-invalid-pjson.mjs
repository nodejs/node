import { checkoutEOL, mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';

import spawn from './helper.spawnAsPromised.mjs';


const entry = fixtures.path('/es-modules/import-invalid-pjson.mjs');
const invalidJson = fixtures.path('/node_modules/invalid-pjson/package.json');

spawn(execPath, [entry])
  .then(mustCall(({ code, signal, stderr }) => {
    assert.ok(
      stderr.includes(
        `[ERR_INVALID_PACKAGE_CONFIG]: Invalid package config ${invalidJson} ` +
        `while importing "invalid-pjson" from ${entry}. ` +
        `Unexpected token } in JSON at position ${12 + checkoutEOL.length * 2}`
      ),
      stderr
    );
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  }));
