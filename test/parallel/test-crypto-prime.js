'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const {
  generatePrime,
  generatePrimeSync,
  checkPrime,
  checkPrimeSync,
} = require('crypto');

const { promisify } = require('util');
const pgeneratePrime = promisify(generatePrime);
const pCheckPrime = promisify(checkPrime);

['hello', false, {}, []].forEach((i) => {
  assert.throws(() => generatePrime(i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => generatePrimeSync(i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

['hello', false, 123].forEach((i) => {
  assert.throws(() => generatePrime(80, i, common.mustNotCall()), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => generatePrimeSync(80, i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

['hello', false, 123].forEach((i) => {
  assert.throws(() => generatePrime(80, {}), {
    code: 'ERR_INVALID_CALLBACK'
  });
});

[-1, 0].forEach((i) => {
  assert.throws(() => generatePrime(i, common.mustNotCall()), {
    code: 'ERR_OUT_OF_RANGE'
  });
  assert.throws(() => generatePrimeSync(i), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

['test', -1, {}, []].forEach((i) => {
  assert.throws(() => generatePrime(8, { safe: i }, common.mustNotCall()), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => generatePrime(8, { rem: i }, common.mustNotCall()), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => generatePrime(8, { add: i }, common.mustNotCall()), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => generatePrimeSync(8, { safe: i }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => generatePrimeSync(8, { rem: i }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => generatePrimeSync(8, { add: i }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

generatePrime(80, common.mustSucceed((prime) => {
  assert(checkPrimeSync(prime));
  checkPrime(prime, common.mustSucceed((result) => {
    assert(result);
  }));
}));

assert(checkPrimeSync(generatePrimeSync(80)));

generatePrime(80, {}, common.mustSucceed((prime) => {
  assert(checkPrimeSync(prime));
}));

assert(checkPrimeSync(generatePrimeSync(80, {})));

generatePrime(32, { safe: true }, common.mustSucceed((prime) => {
  assert(checkPrimeSync(prime));
  const buf = Buffer.from(prime);
  const val = buf.readUInt32BE();
  const check = (val - 1) / 2;
  buf.writeUInt32BE(check);
  assert(checkPrimeSync(buf));
}));

{
  const prime = generatePrimeSync(32, { safe: true });
  assert(checkPrimeSync(prime));
  const buf = Buffer.from(prime);
  const val = buf.readUInt32BE();
  const check = (val - 1) / 2;
  buf.writeUInt32BE(check);
  assert(checkPrimeSync(buf));
}

const add = 12;
const rem = 11;
const add_buf = Buffer.from([add]);
const rem_buf = Buffer.from([rem]);
generatePrime(
  32,
  { add: add_buf, rem: rem_buf },
  common.mustSucceed((prime) => {
    assert(checkPrimeSync(prime));
    const buf = Buffer.from(prime);
    const val = buf.readUInt32BE();
    assert.strictEqual(val % add, rem);
  }));

{
  const prime = generatePrimeSync(32, { add: add_buf, rem: rem_buf });
  assert(checkPrimeSync(prime));
  const buf = Buffer.from(prime);
  const val = buf.readUInt32BE();
  assert.strictEqual(val % add, rem);
}

{
  const prime = generatePrimeSync(32, { add: BigInt(add), rem: BigInt(rem) });
  assert(checkPrimeSync(prime));
  const buf = Buffer.from(prime);
  const val = buf.readUInt32BE();
  assert.strictEqual(val % add, rem);
}

{
  // The behavior when specifying only add without rem should depend on the
  // safe option.

  if (process.versions.openssl >= '1.1.1f') {
    generatePrime(128, {
      bigint: true,
      add: 5n
    }, common.mustSucceed((prime) => {
      assert(checkPrimeSync(prime));
      assert.strictEqual(prime % 5n, 1n);
    }));

    generatePrime(128, {
      bigint: true,
      safe: true,
      add: 5n
    }, common.mustSucceed((prime) => {
      assert(checkPrimeSync(prime));
      assert.strictEqual(prime % 5n, 3n);
    }));
  }
}

[1, 'hello', {}, []].forEach((i) => {
  assert.throws(() => checkPrime(i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

['hello', {}, []].forEach((i) => {
  assert.throws(() => checkPrime(2, { checks: i }), {
    code: 'ERR_INVALID_ARG_TYPE'
  }, common.mustNotCall());
  assert.throws(() => checkPrimeSync(2, { checks: i }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

assert(!checkPrimeSync(Buffer.from([0x1])));
assert(checkPrimeSync(Buffer.from([0x2])));
assert(checkPrimeSync(Buffer.from([0x3])));
assert(!checkPrimeSync(Buffer.from([0x4])));

assert(
  !checkPrimeSync(
    Buffer.from([0x1]),
    {
      fast: true,
      trialDivision: true,
      checks: 10
    }));

(async function() {
  const prime = await pgeneratePrime(36);
  assert(await pCheckPrime(prime));
})().then(common.mustCall());

assert.throws(() => {
  generatePrimeSync(32, { bigint: '' });
}, { code: 'ERR_INVALID_ARG_TYPE' });

assert.throws(() => {
  generatePrime(32, { bigint: '' }, common.mustNotCall());
}, { code: 'ERR_INVALID_ARG_TYPE' });

{
  const prime = generatePrimeSync(3, { bigint: true });
  assert.strictEqual(typeof prime, 'bigint');
  assert.strictEqual(prime, 7n);
  assert(checkPrimeSync(prime));
  checkPrime(prime, common.mustSucceed(assert));
}

{
  generatePrime(3, { bigint: true }, common.mustSucceed((prime) => {
    assert.strictEqual(typeof prime, 'bigint');
    assert.strictEqual(prime, 7n);
    assert(checkPrimeSync(prime));
    checkPrime(prime, common.mustSucceed(assert));
  }));
}
