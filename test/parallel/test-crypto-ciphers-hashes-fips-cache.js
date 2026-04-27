// Flags: --expose-internals
'use strict';

// Verify that getCiphers() and getHashes() reflect the current FIPS state
// rather than returning a stale cached snapshot from before setFips() was
// called.  Regression test for https://github.com/nodejs/node/issues/62982.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { internalBinding } = require('internal/test/binding');
const { testFipsCrypto } = internalBinding('crypto');

if (!testFipsCrypto())
  common.skip('FIPS not supported in this build');

const assert = require('assert');
const { getCiphers, getHashes, setFips, getFips } = require('crypto');

const initialFips = getFips();

// Ensure FIPS is off so we can capture the full algorithm lists as a baseline,
// regardless of whether the system has FIPS on by default.
if (initialFips)
  setFips(false);

const ciphersWithoutFips = getCiphers();
const hashesWithoutFips = getHashes();

assert.ok(ciphersWithoutFips.length > 0, 'expected at least one cipher');
assert.ok(hashesWithoutFips.length > 0, 'expected at least one hash');

// Switch to FIPS mode; the lists must be re-derived, not served from cache.
setFips(true);
assert.strictEqual(getFips(), 1);

const ciphersWithFips = getCiphers();
const hashesWithFips = getHashes();

// FIPS mode restricts the visible algorithm set — the lists must shrink
// (or at minimum differ; some platforms expose only FIPS algorithms by
// default, but in that case the full list can't be larger than the FIPS one).
assert.ok(
  ciphersWithFips.length <= ciphersWithoutFips.length,
  `Expected FIPS cipher list (${ciphersWithFips.length}) to be no larger ` +
  `than the full list (${ciphersWithoutFips.length})`
);
assert.ok(
  hashesWithFips.length <= hashesWithoutFips.length,
  `Expected FIPS hash list (${hashesWithFips.length}) to be no larger ` +
  `than the full list (${hashesWithoutFips.length})`
);

// Every FIPS-mode algorithm must also appear in the non-FIPS list.
for (const cipher of ciphersWithFips) {
  assert.ok(
    ciphersWithoutFips.includes(cipher),
    `FIPS cipher '${cipher}' missing from the non-FIPS list`
  );
}
for (const hash of hashesWithFips) {
  assert.ok(
    hashesWithoutFips.includes(hash),
    `FIPS hash '${hash}' missing from the non-FIPS list`
  );
}

// Turn FIPS back off; the cache must be evicted so the full lists come back.
setFips(false);
assert.strictEqual(getFips(), 0);

assert.deepStrictEqual(
  getCiphers(),
  ciphersWithoutFips,
  'getCiphers() should match pre-FIPS list after setFips(false)'
);
assert.deepStrictEqual(
  getHashes(),
  hashesWithoutFips,
  'getHashes() should match pre-FIPS list after setFips(false)'
);

// Restore the original FIPS state.
if (initialFips)
  setFips(true);
