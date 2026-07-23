import * as common from '../common/index.mjs';
import {
  hash,
  createHash,
  createHmac,
  getHashes,
  generateKeyPairSync,
  createSign,
  createVerify,
  pbkdf2,
  pbkdf2Sync,
  hkdf,
  hkdfSync,
  scrypt,
  scryptSync,
  sign,
  verify,
  argon2,
  argon2Sync,
  createECDH,
  randomBytes,
  timingSafeEqual,
} from 'node:crypto';
import assert from 'node:assert';
import { promisify } from 'node:util';
import { describe, test } from 'node:test';

if (!common.hasCrypto) common.skip('missing crypto');

const bad = [
  ...['\ud800', 'hello\ud800world', 'whatever\ud800'],
  ...['\udbff', 'hello\udbffworld', 'whatever\udbff'],
  ...['\udc00', 'hello\udc00world', 'whatever\udc00'],
  ...['\udfff', 'hello\udfffworld', 'whatever\udfff'],
  ...['\ud800\ud800', 'hello\ud800\ud800world', 'whatever\ud800\ud800'],
  ...['\udfff\ud800', 'hello\udfff\ud800world', 'whatever\udfff\ud800'],
  ...['\udfff\udfff', 'hello\udfff\udfffworld', 'whatever\udfff\udfff'],
];

const good = [
  '',
  ...['\ufffd\ufffd', '\ufffd\ufffd\ufffd'], // To recheck against multiple replacements in x + y and x + y + z from bad
  ...[' ', 'hello world', 'whatever '],
  ...['\ufffd', 'hello\uffddworld', 'whatever\uffdd'],
  ...['\ue000', 'hello\ue000world', 'whatever\ue000'],
  ...['\ud800\udfff', 'hello\ud800\udfffworld', 'whatever\ud800\udfff'],
];

const all = [...good, ...bad];
const all2 = all.flatMap((x) => all.map((y) => [x, y]));
const good2 = good.flatMap((x) => good.map((y) => [x, y]));
const good3 = good.flatMap((x) => good2.map(([y, z]) => [x, y, z]));
const bad2 = [
  ...all.flatMap((x) => bad.map((y) => [x, y])),
  ...bad.flatMap((x) => good.map((y) => [x, y])),
];
const bad3 = [
  ...all.flatMap((x) => bad2.map(([y, z]) => [x, y, z])),
  ...bad.flatMap((x) => good2.map(([y, z]) => [x, y, z])),
];

// algos

const hashes = [
  'sha1',
  ...['sha224', 'sha256', 'sha384', 'sha512'],
  ...(!process.features.openssl_is_boringssl ? ['sha3-224', 'sha3-256', 'sha3-384', 'sha3-512'] : []),
];

const hashesFast = ['sha256'];

// Adding more curves slows down this test, so we only test one of each type
const signing = [
  { type: 'rsa', options: { modulusLength: 1024 } },
  { type: 'ec', options: { namedCurve: 'P-256' } },
  { type: 'ed25519', hashes: [null] },
];

const expectedError = Error; // TODO

async function testOneArg(f) {
  assert(f.length === 1 || f.length === 2);
  if (f.length === 2) f = promisify(f);
  for (const x of good) await f(x);
  for (const x of bad) await assert.rejects(async () => f(x), expectedError);
}

async function testTwoArgs(f) {
  assert(f.length === 2 || f.length === 3);
  if (f.length === 3) f = promisify(f);
  for (const [x, y] of good2) await f(x, y);
  for (const [x, y] of bad2) await assert.rejects(async () => f(x, y), expectedError);
}

async function testThreeArgs(f) {
  assert(f.length === 3 || f.length === 4);
  if (f.length === 4) f = promisify(f);
  for (const [x, y, z] of good3) await f(x, y, z);
  for (const [x, y, z] of bad3) await assert.rejects(async () => f(x, y, z), expectedError);
}

describe('hash()', () => {
  test('Should refuse invalid input', async () => {
    for (const algo of getHashes()) {
      await testOneArg((x) => hash(algo, x));
    }
  });

  test('Equal hashes must mean equal input', (t) => {
    for (const algo of getHashes()) {
      for (const x of good) {
        const hx = hash(algo, x);
        t.assert.strictEqual(typeof hx, 'string');
        for (const y of all) {
          let hy;
          try {
            hy = hash(algo, y);
          } catch {
            continue;
          }
          t.assert.strictEqual(hx === hy, x === y);
        }
      }
    }
  });
});

describe('createHash()', () => {
  test('Should refuse invalid input', async () => {
    for (const algo of hashes) {
      await testOneArg((x) => createHash(algo).update(x).digest());
      await testTwoArgs((x, y) => createHash(algo).update(x).update(y).digest());
    }

    // all other algos
    for (const algo of getHashes()) {
      if (hashes.includes(algo)) continue; // already checked
      await testOneArg((x) => createHash(algo).update(x).digest());
      await testTwoArgs((x, y) => createHash(algo).update(x).update(y).digest());
    }
  });

  test('Equal hashes must mean equal input: one-arg version', (t) => {
    for (const algo of getHashes()) {
      for (const x of good) {
        const hx = createHash(algo).update(x).digest();
        t.assert.ok(hx instanceof Buffer);
        for (const y of all) {
          let hy;
          try {
            hy = createHash(algo).update(y).digest();
          } catch {
            continue;
          }
          t.assert.strictEqual(Buffer.compare(hx, hy) === 0, x === y);
        }
      }
    }
  });

  test('Equal hashes must mean equal input: two-arg version', (t) => {
    for (const algo of hashesFast) {
      for (const x of good) {
        const hx = createHash(algo).update(x).digest();
        t.assert.ok(hx instanceof Buffer);
        for (const [y, z] of all2) {
          let hy;
          try {
            hy = createHash(algo).update(y).update(z).digest();
          } catch {
            continue;
          }
          if (Buffer.compare(hx, hy) === 0) t.assert.strictEqual(x, y + z); // The other way around is not always true
        }
      }
    }
  });
});

describe('createHmac()', () => {
  test('Should refuse invalid input', async () => {
    for (const algo of hashes) {
      await testTwoArgs((x, y) => createHmac(algo, x).update(y).digest());
    }

    // All other algos that don't throw
    for (const algo of getHashes()) {
      if (hashes.includes(algo)) continue; // already checked
      try {
        createHmac(algo, 'key');
      } catch {
        // e.g. shake
        continue;
      }

      await testTwoArgs((x, y) => createHmac(algo, x).update(y).digest());
    }

    // Three-arg version
    for (const algo of hashesFast) {
      await testThreeArgs((x, y, z) => createHmac(algo, x).update(y).update(z).digest());
    }
  });
});

describe('pbkdf2()', () => {
  const iterations = 1;
  const size = 32;

  test('Should refuse invalid input', async () => {
    for (const algo of hashesFast) {
      await testTwoArgs((x, y) => pbkdf2Sync(x, y, iterations, size, algo));
    }
  });

  test('Should refuse invalid input, callbacks', async () => {
    for (const algo of hashesFast) {
      await testTwoArgs((x, y, cb) => pbkdf2(x, y, iterations, size, algo, cb));
    }
  });

  test('Equal hashes must mean equal input', (t) => {
    for (const algo of hashesFast) {
      for (const [x, y] of good2) {
        const hx = pbkdf2Sync(x, y, iterations, size, algo);
        t.assert.ok(hx instanceof Buffer);
        for (const z of all) {
          let xz;
          try {
            xz = pbkdf2Sync(x, z, iterations, size, algo, size);
          } catch {
            continue;
          }
          t.assert.strictEqual(Buffer.compare(hx, xz) === 0, y === z);
        }
        for (const z of all) {
          let zy;
          try {
            zy = pbkdf2Sync(z, y, iterations, size, algo, size);
          } catch {
            continue;
          }
          t.assert.strictEqual(Buffer.compare(hx, zy) === 0, x === z);
        }
      }
    }
  });
});

describe('hkdf()', () => {
  const size = 32;

  test('Should refuse invalid input', async () => {
    for (const algo of hashesFast) {
      // Three-arg is slow, so we do this
      await testTwoArgs((x, y) => hkdfSync(algo, x, y, '', size));
      await testTwoArgs((x, y) => hkdfSync(algo, x, 'salt', y, size));
    }
  });

  test('Should refuse invalid input, callbacks', async () => {
    for (const algo of hashesFast) {
      // Three-arg is slow, so we do this
      await testTwoArgs((x, y, cb) => hkdf(algo, x, y, '', size, cb));
      await testTwoArgs((x, y, cb) => hkdf(algo, x, 'salt', y, size, cb));
    }
  });

  test('Equal hashes must mean equal input', (t) => {
    for (const algo of hashesFast) {
      for (const [x, y] of good2) {
        const hx = hkdfSync(algo, x, y, '', size);
        t.assert.ok(hx instanceof ArrayBuffer);
        for (const z of all) {
          let xz;
          try {
            xz = hkdfSync(algo, x, z, '', size);
          } catch {
            continue;
          }
          t.assert.strictEqual(Buffer.compare(Buffer.from(hx), Buffer.from(xz)) === 0, y === z);
        }
        for (const z of all) {
          let zy;
          try {
            zy = hkdfSync(algo, z, y, '', size);
          } catch {
            continue;
          }
          t.assert.strictEqual(Buffer.compare(Buffer.from(hx), Buffer.from(zy)) === 0, x === z);
        }
      }
    }
  });
});

describe('scrypt()', () => {
  const size = 32;
  const params = { cost: 2, blockSize: 1 };

  test('Should refuse invalid input', async () => {
    await testTwoArgs((x, y) => scryptSync(x, y, size, params));
  });

  test('Should refuse invalid input, callbacks', async () => {
    await testTwoArgs((x, y, cb) => scrypt(x, y, size, params, cb));
  });

  test('Equal hashes must mean equal input', (t) => {
    for (const [x, y] of good2) {
      const hx = scryptSync(x, y, size, params);
      t.assert.ok(hx instanceof Buffer);
      for (const z of all) {
        let xz;
        try {
          xz = scryptSync(x, z, size, params);
        } catch {
          continue;
        }
        t.assert.strictEqual(Buffer.compare(hx, xz) === 0, y === z);
      }
      for (const z of all) {
        let zy;
        try {
          zy = scryptSync(z, y, size, params);
        } catch {
          continue;
        }
        t.assert.strictEqual(Buffer.compare(hx, zy) === 0, x === z);
      }
    }
  });
});

describe('createSign()', () => {
  test('Should refuse invalid input', async () => {
    for (const { type, options, hashes: useHashes } of signing) {
      const { privateKey } = generateKeyPairSync(type, options);
      for (const algo of useHashes ?? [null, ...hashes]) {
        if (algo === null) continue; // Can't use that in createSign / createVerify
        await testOneArg((x) => createSign(algo).update(x).end().sign(privateKey));
        await testTwoArgs((x, y) => createSign(algo).update(x).update(y).end().sign(privateKey));
      }
    }
  });
});

describe('createVerify()', () => {
  test('Should refuse invalid input', async () => {
    for (const { type, options, hashes: useHashes } of signing) {
      const { privateKey, publicKey } = generateKeyPairSync(type, options);
      for (const algo of useHashes ?? [null, ...hashes]) {
        if (algo === null) continue; // Can't use that in createSign / createVerify
        const signature = createSign(algo).update('').end().sign(privateKey);
        await testOneArg((x) => createVerify(algo).update(x).end().verify(publicKey, signature));
        await testTwoArgs((x, y) => {
          createVerify(algo).update(x).update(y).end().verify(publicKey, signature);
        });
      }
    }
  });

  test('Should never verify falsely: one-arg version', (t) => {
    for (const { type, options, hashes: useHashes } of signing) {
      const { privateKey, publicKey } = generateKeyPairSync(type, options);
      for (const algo of useHashes ?? [null, ...hashes]) {
        if (algo === null) continue; // Can't use that in createSign / createVerify
        for (const x of good) {
          const signature = createSign(algo).update(x).end().sign(privateKey);
          for (const y of all) {
            let verified;
            try {
              verified = createVerify(algo).update(y).end().verify(publicKey, signature);
            } catch {
              verified = false;
            }
            t.assert.strictEqual(verified, x === y);
          }
        }
      }
    }
  });

  test('Should never verify falsely: two-arg version', (t) => {
    for (const { type, options, hashes: useHashes } of signing) {
      const { privateKey, publicKey } = generateKeyPairSync(type, options);
      for (const algo of useHashes ?? [null, ...hashesFast]) {
        if (algo === null) continue; // Can't use that in createSign / createVerify
        for (const x of good) {
          const signature = createSign(algo).update(x).end().sign(privateKey);
          for (const [y, z] of all2) {
            let verified;
            try {
              verified = createVerify(algo).update(y).update(z).end().verify(publicKey, signature);
            } catch {
              verified = false;
            }
            if (verified) t.assert.strictEqual(x, y + z); // The other way around is not always true
          }
        }
      }
    }
  });
});

describe('sign()', () => {
  test('Should refuse invalid input', async () => {
    for (const { type, options, hashes: useHashes } of signing) {
      const { privateKey } = generateKeyPairSync(type, options);
      for (const algo of useHashes ?? [null, ...hashesFast]) {
        if (algo === null) continue; // Can't use that in createSign / createVerify
        await testOneArg((x) => sign(algo, x, privateKey));
      }
    }
  });
});

describe('verify()', () => {
  test('Should refuse invalid input', async () => {
    for (const { type, options, hashes: useHashes } of signing) {
      const { privateKey, publicKey } = generateKeyPairSync(type, options);
      for (const algo of useHashes ?? [null, ...hashes]) {
        const signature = sign(algo, '', privateKey);
        await testOneArg((x) => verify(algo, x, publicKey, signature));
      }
    }
  });

  test('Should never verify falsely', (t) => {
    for (const { type, options, hashes: useHashes } of signing) {
      const { privateKey, publicKey } = generateKeyPairSync(type, options);
      for (const algo of useHashes ?? [null, ...hashes]) {
        for (const x of good) {
          const signature = sign(algo, x, privateKey);
          for (const y of all) {
            let verified;
            try {
              verified = verify(algo, y, publicKey, signature);
            } catch {
              verified = false;
            }
            t.assert.strictEqual(verified, x === y);
          }
        }
      }
    }
  });
});

describe('argon2()', () => {
  const options = {
    message: '', // Default for nonce tests
    nonce: randomBytes(8), // salt
    parallelism: 1,
    tagLength: 8,
    memory: 8,
    passes: 1,
  };

  test('Should refuse invalid input', async () => {
    for (const algo of ['argon2d', 'argon2i', 'argon2id']) {
      await testOneArg((x) => argon2Sync(algo, { ...options, message: x }));
      await testOneArg((x) => argon2Sync(algo, { ...options, nonce: '12345678' + x }));
    }
  });

  test('Should refuse invalid input, callbacks', async () => {
    for (const algo of ['argon2d', 'argon2i', 'argon2id']) {
      await testOneArg((x, cb) => argon2(algo, { ...options, message: x }, cb));
      await testOneArg((x, cb) => argon2(algo, { ...options, nonce: '12345678' + x }, cb));
    }
  });

  test('Equal hashes must mean equal input', (t) => {
    for (const algo of ['argon2d']) { // Only test one for perf
      for (const x of good) {
        const hx = argon2Sync(algo, { ...options, message: x });
        t.assert.ok(hx instanceof Buffer);
        for (const y of all) {
          if (x === y) continue;
          let hy;
          try {
            hy = argon2Sync(algo, { ...options, message: y });
          } catch {
            continue;
          }
          t.assert.notDeepEqual(hx, hy); // we skipped x === y
        }
      }
    }
  });
});

describe('createECDH()', () => {
  const curve = 'secp256k1';

  test('Should refuse invalid input', (t) => {
    for (const x of bad) {
      t.assert.throws(() => createECDH(curve).setPrivateKey(x), expectedError);
      t.assert.throws(() => createECDH(curve).setPublicKey(x), expectedError);
    }
  });
});

// Throws on strings, but let's recheck against regressions just in case
describe('timingSafeEqual()', () => {
  test('Should refuse invalid input', (t) => {
    for (const [x, y] of bad2) t.assert.throws(() => timingSafeEqual(x, y), Error); // any error
  });
});
