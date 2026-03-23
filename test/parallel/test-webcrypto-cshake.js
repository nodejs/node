'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(4))
  common.skip('requires OpenSSL >= 4.0 for cSHAKE customization support');

const assert = require('assert');
const { subtle } = globalThis.crypto;

const encode = (str) => new TextEncoder().encode(str);

// Test cSHAKE with customization and functionName (OpenSSL 4.0+)
// Vectors generated with OpenSSL 4.0 CSHAKE-128/CSHAKE-256 implementation.
// The first vector matches NIST SP 800-185 sample #3.
(async () => {
  const input = new Uint8Array([0x00, 0x01, 0x02, 0x03]);
  const emptyInput = new Uint8Array(0);

  // cSHAKE128 with customization - matches NIST SP 800-185 sample #3
  {
    const result = await subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      customization: encode('Email Signature'),
    }, input);
    assert.strictEqual(
      Buffer.from(result).toString('hex'),
      'c1c36925b6409a04f1b504fcbca9d82b4017277cb5ed2b2065fc1d3814d5aaf5',
    );
  }

  // cSHAKE256 with customization
  {
    const result = await subtle.digest({
      name: 'cSHAKE256',
      outputLength: 512,
      customization: encode('Email Signature'),
    }, input);
    assert.strictEqual(
      Buffer.from(result).toString('hex'),
      'd008828e2b80ac9d2218ffee1d070c48b8e4c87bff32c9699d5b6896eee0edd1' +
      '64020e2be0560858d9c00c037e34a96937c561a74c412bb4c746469527281c8c',
    );
  }

  // cSHAKE128 with empty data and customization
  {
    const result = await subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      customization: encode('Email Signature'),
    }, emptyInput);
    assert.strictEqual(
      Buffer.from(result).toString('hex'),
      '22af17860970726beae182499c8cf8c2f17700f9856d1ea0d01f489c18b5b9d5',
    );
  }

  // cSHAKE256 with empty data and customization
  {
    const result = await subtle.digest({
      name: 'cSHAKE256',
      outputLength: 512,
      customization: encode('Email Signature'),
    }, emptyInput);
    assert.strictEqual(
      Buffer.from(result).toString('hex'),
      'a8dd3ab039e3926f6f22c130ef305c2f47a7fe8eb85f93433961c6fe1637619b' +
      '4c67f87f9c8bc583643cd5943f7acd332eb23f35d027cf2ca85b6c2da8ccbacf',
    );
  }

  // cSHAKE128 with functionName=KMAC
  {
    const result = await subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      functionName: encode('KMAC'),
    }, input);
    assert.strictEqual(
      Buffer.from(result).toString('hex'),
      'be0830bf71d17acd55e891dc475d02a183d9266d4d5f58abf39127e88d60e05d',
    );
  }

  // cSHAKE256 with functionName=KMAC
  {
    const result = await subtle.digest({
      name: 'cSHAKE256',
      outputLength: 512,
      functionName: encode('KMAC'),
    }, input);
    assert.strictEqual(
      Buffer.from(result).toString('hex'),
      '6f94c691c7dae6603dedf3b9be9c15ea7c6dd614ebeb8a56c91d5938fb6174c9' +
      '3d846ac67c21c4ddbe959ee80f27b71ba75938b2f582be741b7e834cfdcb8f09',
    );
  }

  // cSHAKE128 with both functionName=KMAC and customization
  {
    const result = await subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      functionName: encode('KMAC'),
      customization: encode('My Tagged Application'),
    }, input);
    assert.strictEqual(
      Buffer.from(result).toString('hex'),
      '89279d43af74f2851418d260808fbcf7a07f938e6b973cdc29e00e7fcc0f1aab',
    );
  }

  // cSHAKE256 with both functionName=KMAC and customization
  {
    const result = await subtle.digest({
      name: 'cSHAKE256',
      outputLength: 512,
      functionName: encode('KMAC'),
      customization: encode('My Tagged Application'),
    }, input);
    assert.strictEqual(
      Buffer.from(result).toString('hex'),
      'c2456362309449cf95fb15ce277e084baf2bd84f7c6b9df690a11bc3c8e9742b' +
      '1c4072e2155d857b5ee126479c9bc5ad1c7a0d346d84012843d0cb0f6f30acb6',
    );
  }

  // Empty customization produces same output as no customization (plain SHAKE)
  {
    const [r1, r2] = await Promise.all([
      subtle.digest({ name: 'cSHAKE128', outputLength: 256 }, input),
      subtle.digest({
        name: 'cSHAKE128',
        outputLength: 256,
        customization: new Uint8Array(0),
      }, input),
    ]);
    assert.deepStrictEqual(new Uint8Array(r1), new Uint8Array(r2));
  }

  // Empty functionName produces same output as no functionName
  {
    const [r1, r2] = await Promise.all([
      subtle.digest({ name: 'cSHAKE256', outputLength: 512 }, input),
      subtle.digest({
        name: 'cSHAKE256',
        outputLength: 512,
        functionName: new Uint8Array(0),
      }, input),
    ]);
    assert.deepStrictEqual(new Uint8Array(r1), new Uint8Array(r2));
  }

  // Invalid functionName is rejected by OpenSSL at operation time.
  await assert.rejects(
    subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      functionName: encode('InvalidName'),
    }, input),
    { name: 'OperationError' },
  );

  // OpenSSL documents a 512-byte customization limit, but currently rejects
  // that boundary value at operation time.
  await assert.rejects(
    subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      customization: new Uint8Array(512).fill(1),
    }, input),
    { name: 'OperationError' },
  );

  // Customization at exactly 511 bytes (boundary, should succeed)
  {
    const result = await subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      customization: new Uint8Array(511).fill(1),
    }, input);
    assert(result instanceof ArrayBuffer);
    assert.strictEqual(result.byteLength, 32);
  }

  // Customization is passed through to OpenSSL.
  {
    const [r1, r2] = await Promise.all([
      subtle.digest({
        name: 'cSHAKE128',
        outputLength: 256,
        customization: Uint8Array.of(0x41),
      }, input),
      subtle.digest({
        name: 'cSHAKE128',
        outputLength: 256,
        customization: Uint8Array.of(0x41, 0xff, 0x42),
      }, input),
    ]);
    assert.notDeepStrictEqual(new Uint8Array(r1), new Uint8Array(r2));
  }

  // All valid functionName values produce results
  for (const name of ['KMAC', 'TupleHash', 'ParallelHash']) {
    const result = await subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      functionName: encode(name),
    }, input);
    assert(result instanceof ArrayBuffer);
    assert.strictEqual(result.byteLength, 32);
  }

  // Different BufferSource types for customization produce same result
  {
    const customization = encode('test');
    const result1 = await subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      customization,
    }, input);
    const result2 = await subtle.digest({
      name: 'cSHAKE128',
      outputLength: 256,
      customization: customization.buffer,
    }, input);
    assert.deepStrictEqual(new Uint8Array(result1), new Uint8Array(result2));
  }
})().then(common.mustCall());
