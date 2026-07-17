// Flags: --experimental-shadow-realm
'use strict';

require('../common');
const { intrinsics, webIdlExposedWildcard } = require('../common/globals');
const assert = require('assert');

// Validates APIs exposed on the ShadowRealm globalThis.
const shadowRealm = new ShadowRealm();
const itemsStr = shadowRealm.evaluate(`
(() => {
  return Object.getOwnPropertyNames(globalThis).join(',');
})();
`);
const items = itemsStr.split(',');
const leaks = [];
for (const item of items) {
  if (intrinsics.has(item)) {
    continue;
  }
  if (webIdlExposedWildcard.has(item)) {
    continue;
  }
  leaks.push(item);
}

assert.deepStrictEqual(leaks, []);
