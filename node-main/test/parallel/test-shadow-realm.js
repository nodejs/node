// Flags: --experimental-shadow-realm
'use strict';

require('../common');
const assert = require('assert');

// Validates we can construct ShadowRealm successfully.
const shadowRealm = new ShadowRealm();

const getter = shadowRealm.evaluate('globalThis.realmValue = "inner"; () => globalThis.realmValue;');
assert.strictEqual(getter(), 'inner');
assert.strictEqual('realmValue' in globalThis, false);
