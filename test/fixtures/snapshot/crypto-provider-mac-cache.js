'use strict';

const assert = require('node:assert');
const {
  createMac,
  getFips,
  getMacs,
  setFips,
} = require('node:crypto');
const { setDeserializeMainFunction } = require('node:v8').startupSnapshot;

const algorithm = 'poly1305';
const key = Buffer.from(
  '85d6be7857556d337f4452fe42d506a8' +
  '0103808afb0db2fd4abff6af4149f51b',
  'hex',
);
const data = Buffer.from('Cryptographic Forum Research Group');
const expected = 'a8061dc1305136c6c22b8baf0c0127a9';

setFips(0);
assert(getMacs().includes(algorithm));
assert.strictEqual(
  createMac(algorithm, key).update(data).final('hex'),
  expected,
);

setDeserializeMainFunction(() => {
  // Resolve through the JavaScript alias cache before refreshing getMacs().
  // Startup snapshot serialization must not retain the build-time cache IDs.
  assert.strictEqual(
    createMac(algorithm, key).update(data).final('hex'),
    expected,
  );
  const expectedMacs = getMacs();
  assert(expectedMacs.includes(algorithm));
  const disposableMacs = getMacs();
  disposableMacs.length = 0;
  assert.deepStrictEqual(getMacs(), expectedMacs);
  assert.strictEqual(
    createMac(algorithm, key).update(data).final('hex'),
    expected,
  );

  let toggled = false;
  try {
    setFips(1);
    toggled = getFips() === 1;
  } catch {
    // FIPS mode is optional; snapshot cache rebuilding is still covered.
  }
  if (toggled && !getMacs().includes(algorithm)) {
    assert.throws(() => createMac(algorithm, key), {
      code: 'ERR_CRYPTO_INVALID_MAC',
    });
  }
  setFips(0);

  assert(getMacs().includes(algorithm));
  assert.strictEqual(
    createMac(algorithm, key).update(data).final('hex'),
    expected,
  );
  console.log('provider MAC cache snapshot: ok');
});
