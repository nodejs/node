// Flags: --experimental-shadow-realm --max-old-space-size=20
'use strict';

/**
 * Verifying ShadowRealm instances can be correctly garbage collected.
 */

require('../common');

for (let i = 0; i < 1000; i++) {
  new ShadowRealm();
}
