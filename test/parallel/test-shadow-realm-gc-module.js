// Flags: --experimental-shadow-realm --max-old-space-size=20
'use strict';

/**
 * Verifying modules imported by ShadowRealm instances can be correctly
 * garbage collected.
 */

const common = require('../common');
const fixtures = require('../common/fixtures');

async function main() {
  const mod = fixtures.fileURL('es-module-shadow-realm', 'state-counter.mjs');
  for (let i = 0; i < 100; i++) {
    const realm = new ShadowRealm();
    await realm.importValue(mod, 'getCounter');
  }
}

main().then(common.mustCall());
