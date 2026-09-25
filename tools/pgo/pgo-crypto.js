'use strict';

// PGO Training Script: Crypto Operations
//
// Exercises the most common crypto operations in Node.js applications:
// - TLS/HTTPS is used in virtually every production deployment
// - Password hashing (bcrypt-like patterns via pbkdf2/scrypt)
// - HMAC for API authentication (AWS Signature, JWT)
// - SHA-256/SHA-512 hashing for checksums, ETags, content-addressing
// - AES-GCM encryption for data-at-rest
// - Random byte generation (session tokens, UUIDs, nonces)
// - Certificate/key handling patterns
//
// This exercises: OpenSSL (via Node.js crypto binding), Buffer allocation,
// C++ ↔ JS boundary crossing, async crypto operations, KeyObject handling.

const crypto = require('crypto');
const { subtle } = globalThis.crypto;

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;

// Test data of varying sizes
const DATA_TINY = Buffer.from('Hello, World!');
const DATA_SMALL = crypto.randomBytes(256);
const DATA_MEDIUM = crypto.randomBytes(4096);
const DATA_LARGE = crypto.randomBytes(65536);
const DATA_XLARGE = crypto.randomBytes(262144); // 256 KB
const DATA_SIZES = [
  DATA_TINY,
  DATA_SMALL,
  DATA_MEDIUM,
  DATA_LARGE,
  DATA_XLARGE,
];

// Pre-generated keys for symmetric encryption
const AES_KEY = crypto.randomBytes(32); // AES-256
const HMAC_KEY = crypto.randomBytes(64);

// RSA key pair (pre-generated for speed)
const { publicKey: RSA_PUBLIC, privateKey: RSA_PRIVATE } =
  crypto.generateKeyPairSync('rsa', {
    modulusLength: 2048,
    publicKeyEncoding: { type: 'spki', format: 'pem' },
    privateKeyEncoding: { type: 'pkcs8', format: 'pem' },
  });

// EC key pair
const { publicKey: EC_PUBLIC, privateKey: EC_PRIVATE } =
  crypto.generateKeyPairSync('ec', {
    namedCurve: 'P-256',
    publicKeyEncoding: { type: 'spki', format: 'pem' },
    privateKeyEncoding: { type: 'pkcs8', format: 'pem' },
  });

// Ed25519 for modern signing
const { publicKey: ED_PUBLIC, privateKey: ED_PRIVATE } =
  crypto.generateKeyPairSync('ed25519', {
    publicKeyEncoding: { type: 'spki', format: 'pem' },
    privateKeyEncoding: { type: 'pkcs8', format: 'pem' },
  });

// Workload 1: Hashing (most common crypto operation)
function workloadHashing(iterations) {
  const algorithms = ['sha256', 'sha384', 'sha512', 'sha1', 'md5'];
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    for (const algo of algorithms) {
      const data = DATA_SIZES[i % DATA_SIZES.length];

      // One-shot hash (most common pattern: ETags, checksums)
      crypto.createHash(algo).update(data).digest('hex');
      ops++;

      // Streaming hash (file hashing pattern)
      if (i % 5 === 0) {
        const hash = crypto.createHash(algo);
        const chunkSize = 1024;
        for (let offset = 0; offset < data.length; offset += chunkSize) {
          hash.update(
            data.subarray(offset, Math.min(offset + chunkSize, data.length)),
          );
        }
        hash.digest('base64');
        ops++;
      }
    }
  }
  return ops;
}

// Workload 2: HMAC (API auth, JWT signatures, webhook verification)
function workloadHMAC(iterations) {
  let ops = 0;
  const algorithms = ['sha256', 'sha384', 'sha512'];

  for (let i = 0; i < iterations; i++) {
    const algo = algorithms[i % algorithms.length];
    const data = DATA_SIZES[i % DATA_SIZES.length];

    // HMAC compute (AWS Signature v4, JWT pattern)
    const sig = crypto.createHmac(algo, HMAC_KEY).update(data).digest();
    ops++;

    // HMAC verify (webhook verification pattern)
    const sig2 = crypto.createHmac(algo, HMAC_KEY).update(data).digest();
    crypto.timingSafeEqual(sig, sig2);
    ops++;
  }
  return ops;
}

// Workload 3: AES-GCM encryption/decryption (data protection)
function workloadAESGCM(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const data = DATA_SIZES[i % DATA_SIZES.length];
    const iv = crypto.randomBytes(12);

    // Encrypt
    const cipher = crypto.createCipheriv('aes-256-gcm', AES_KEY, iv);
    const encrypted = Buffer.concat([cipher.update(data), cipher.final()]);
    const tag = cipher.getAuthTag();
    ops++;

    // Decrypt
    const decipher = crypto.createDecipheriv('aes-256-gcm', AES_KEY, iv);
    decipher.setAuthTag(tag);
    Buffer.concat([decipher.update(encrypted), decipher.final()]);
    ops++;
  }
  return ops;
}

// Workload 4: RSA sign/verify (JWT RS256, code signing)
function workloadRSASignVerify(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const data = DATA_SIZES[Math.min(i % 3, DATA_SIZES.length - 1)]; // RSA limits payload size

    // Sign
    const sign = crypto.createSign('RSA-SHA256');
    sign.update(data);
    const signature = sign.sign(RSA_PRIVATE);
    ops++;

    // Verify
    const verify = crypto.createVerify('RSA-SHA256');
    verify.update(data);
    verify.verify(RSA_PUBLIC, signature);
    ops++;
  }
  return ops;
}

// Workload 5: ECDSA sign/verify (modern TLS, smaller keys)
function workloadECDSA(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const data = DATA_SIZES[i % DATA_SIZES.length];

    const signature = crypto.sign('SHA256', data, EC_PRIVATE);
    ops++;

    crypto.verify('SHA256', data, EC_PUBLIC, signature);
    ops++;
  }
  return ops;
}

// Workload 6: Ed25519 sign/verify (modern fast signing)
function workloadEd25519(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const data = DATA_SIZES[i % DATA_SIZES.length];

    const signature = crypto.sign(null, data, ED_PRIVATE);
    ops++;

    crypto.verify(null, data, ED_PUBLIC, signature);
    ops++;
  }
  return ops;
}

// Workload 7: Random byte generation (tokens, session IDs, UUIDs)
function workloadRandom(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Session tokens
    crypto.randomBytes(32);
    ops++;

    // UUID generation
    crypto.randomUUID();
    ops++;

    // Random integers (for OTP codes, etc.)
    crypto.randomInt(100000, 999999);
    ops++;

    // Fill existing buffer (pool pattern)
    const buf = Buffer.allocUnsafe(64);
    crypto.randomFillSync(buf);
    ops++;
  }
  return ops;
}

// Workload 8: PBKDF2 / Scrypt (password hashing - async)
async function workloadPasswordHashing(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const password = `password_${i}_with_some_length`;
    const salt = crypto.randomBytes(16);

    // PBKDF2 (most common password hashing in Node.js)
    await new Promise((resolve, reject) => {
      crypto.pbkdf2(password, salt, 1000, 64, 'sha512', (err, key) => {
        if (err) reject(err);
        else resolve(key);
      });
    });
    ops++;

    // Scrypt (recommended for new apps)
    if (i % 3 === 0) {
      await new Promise((resolve, reject) => {
        crypto.scrypt(
          password,
          salt,
          64,
          { N: 1024, r: 8, p: 1 },
          (err, key) => {
            if (err) reject(err);
            else resolve(key);
          },
        );
      });
      ops++;
    }
  }
  return ops;
}

// Workload 9: HKDF (key derivation for encryption key rotation)
async function workloadHKDF(iterations) {
  let ops = 0;
  const ikm = crypto.randomBytes(32);
  const salt = crypto.randomBytes(32);
  const info = Buffer.from('encryption-key-v1');

  for (let i = 0; i < iterations; i++) {
    await new Promise((resolve, reject) => {
      crypto.hkdf('sha256', ikm, salt, info, 32, (err, key) => {
        if (err) reject(err);
        else resolve(key);
      });
    });
    ops++;
  }
  return ops;
}

// Workload 10: WebCrypto API (increasingly used in modern Node.js)
async function workloadWebCrypto(iterations) {
  let ops = 0;

  // Generate WebCrypto key
  const key = await subtle.generateKey(
    { name: 'AES-GCM', length: 256 },
    false,
    ['encrypt', 'decrypt'],
  );

  const hmacKey = await subtle.generateKey(
    { name: 'HMAC', hash: 'SHA-256' },
    false,
    ['sign', 'verify'],
  );

  for (let i = 0; i < iterations; i++) {
    const data = DATA_SIZES[i % 3]; // Keep data smaller for WebCrypto
    const iv = crypto.randomBytes(12);

    // AES-GCM encrypt/decrypt via WebCrypto
    const encrypted = await subtle.encrypt({ name: 'AES-GCM', iv }, key, data);
    await subtle.decrypt({ name: 'AES-GCM', iv }, key, encrypted);
    ops += 2;

    // HMAC sign/verify via WebCrypto
    const sig = await subtle.sign('HMAC', hmacKey, data);
    await subtle.verify('HMAC', hmacKey, sig, data);
    ops += 2;

    // SHA-256 digest via WebCrypto
    await subtle.digest('SHA-256', data);
    ops++;
  }
  return ops;
}

// Workload 11: DH key exchange (TLS handshake simulation)
function workloadDH(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // ECDH key exchange (used in every TLS connection)
    const alice = crypto.createECDH('prime256v1');
    const bob = crypto.createECDH('prime256v1');

    alice.generateKeys();
    bob.generateKeys();

    const aliceSecret = alice.computeSecret(bob.getPublicKey());
    bob.computeSecret(alice.getPublicKey());

    // Derive encryption key from shared secret
    crypto.createHash('sha256').update(aliceSecret).digest();
    ops++;
  }
  return ops;
}

async function main() {
  console.log('[pgo-crypto] Starting crypto workload...');
  const startTime = Date.now();
  let totalOps = 0;
  let round = 0;

  const remaining = () => DURATION_MS - (Date.now() - startTime);

  while (remaining() > 0) {
    round++;
    const scale = Math.max(0.1, remaining() / DURATION_MS);
    const iterScale = (base) => Math.max(1, Math.floor(base * scale));

    // Sync workloads (weighted by real-world frequency)
    if (round === 1) console.log('[pgo-crypto] Running hash workloads...');
    totalOps += workloadHashing(iterScale(500));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-crypto] Running HMAC workloads...');
    totalOps += workloadHMAC(iterScale(300));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-crypto] Running AES-GCM workloads...');
    totalOps += workloadAESGCM(iterScale(200));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-crypto] Running random generation...');
    totalOps += workloadRandom(iterScale(500));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-crypto] Running RSA sign/verify...');
    totalOps += workloadRSASignVerify(iterScale(30));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-crypto] Running ECDSA sign/verify...');
    totalOps += workloadECDSA(iterScale(100));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-crypto] Running Ed25519 sign/verify...');
    totalOps += workloadEd25519(iterScale(100));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-crypto] Running DH key exchange...');
    totalOps += workloadDH(iterScale(30));
    if (remaining() <= 0) break;

    // Async workloads
    if (round === 1)
      console.log('[pgo-crypto] Running password hashing (async)...');
    totalOps += await workloadPasswordHashing(iterScale(20));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-crypto] Running HKDF (async)...');
    totalOps += await workloadHKDF(iterScale(50));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-crypto] Running WebCrypto workloads...');
    totalOps += await workloadWebCrypto(iterScale(50));
  }

  const elapsed = (Date.now() - startTime) / 1000;
  console.log(
    `[pgo-crypto] Completed ${totalOps} crypto operations in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s) [${round} rounds]`,
  );
}

main().catch((err) => {
  console.error('[pgo-crypto] Error:', err);
  process.exit(1);
});
