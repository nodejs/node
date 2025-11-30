'use strict';

const { hasSQLite } = require('../common');
const assert = require('node:assert');

// Ensuring that `sessionStorage` is not a getter that throws when built without SQLite.
assert.strictEqual(typeof sessionStorage, hasSQLite ? 'object' : 'undefined');
assert.strictEqual(Object.hasOwn(globalThis, 'sessionStorage'), hasSQLite);
