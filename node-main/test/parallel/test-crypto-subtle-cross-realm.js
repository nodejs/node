'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;
const vm = require('vm');

// Test with same-realm ArrayBuffer
{
  const samerealmData = new Uint8Array([1, 2, 3, 4]).buffer;

  subtle.digest('SHA-256', samerealmData)
    .then(common.mustCall((result) => {
      assert.strictEqual(result.byteLength, 32); // SHA-256 is 32 bytes
    }));
}

// Test with cross-realm ArrayBuffer
{
  const context = vm.createContext({});
  const crossrealmUint8Array = vm.runInContext('new Uint8Array([1, 2, 3, 4])', context);
  const crossrealmBuffer = crossrealmUint8Array.buffer;

  // Verify it's truly cross-realm
  assert.notStrictEqual(
    Object.getPrototypeOf(crossrealmBuffer),
    ArrayBuffer.prototype
  );

  // This should still work, since we're checking structural type
  subtle.digest('SHA-256', crossrealmBuffer)
    .then(common.mustCall((result) => {
      assert.strictEqual(result.byteLength, 32); // SHA-256 is 32 bytes
    }));
}

// Cross-realm SharedArrayBuffer should be handled like any SharedArrayBuffer
{
  const context = vm.createContext({});
  const crossrealmSAB = vm.runInContext('new SharedArrayBuffer(4)', context);
  assert.notStrictEqual(
    Object.getPrototypeOf(crossrealmSAB),
    SharedArrayBuffer.prototype
  );
  Promise.allSettled([
    subtle.digest('SHA-256', new Uint8Array(new SharedArrayBuffer(4))),
    subtle.digest('SHA-256', new Uint8Array(crossrealmSAB)),
  ]).then(common.mustCall((r) => {
    assert.partialDeepStrictEqual(r, [
      { status: 'rejected' },
      { status: 'rejected' },
    ]);
    assert.strictEqual(r[1].reason.message, r[0].reason.message);
  }));
}

// Test with both TypedArray buffer methods
{
  const context = vm.createContext({});
  const crossrealmUint8Array = vm.runInContext('new Uint8Array([1, 2, 3, 4])', context);

  // Test the .buffer property
  subtle.digest('SHA-256', crossrealmUint8Array.buffer)
    .then(common.mustCall((result) => {
      assert.strictEqual(result.byteLength, 32);
    }));

  // Test passing the TypedArray directly (should work both before and after the fix)
  subtle.digest('SHA-256', crossrealmUint8Array)
    .then(common.mustCall((result) => {
      assert.strictEqual(result.byteLength, 32);
    }));
}

// Test with AES-GCM encryption/decryption using cross-realm ArrayBuffer
{
  const context = vm.createContext({});
  const crossRealmBuffer = vm.runInContext('new ArrayBuffer(16)', context);

  // Fill the buffer with some data
  const dataView = new Uint8Array(crossRealmBuffer);
  for (let i = 0; i < dataView.length; i++) {
    dataView[i] = i % 256;
  }

  // Generate a key
  subtle.generateKey({
    name: 'AES-GCM',
    length: 256
  }, true, ['encrypt', 'decrypt'])
    .then(async (key) => {
      // Create an initialization vector
      const iv = crypto.getRandomValues(new Uint8Array(12));

      // Encrypt using the cross-realm ArrayBuffer
      const ciphertext = await subtle.encrypt(
        { name: 'AES-GCM', iv },
        key,
        crossRealmBuffer
      );
        // Decrypt
      const plaintext = await subtle.decrypt(
        { name: 'AES-GCM', iv },
        key,
        ciphertext
      );
        // Verify the decrypted content matches original
      const decryptedView = new Uint8Array(plaintext);
      for (let i = 0; i < dataView.length; i++) {
        assert.strictEqual(
          decryptedView[i],
          dataView[i],
          `Byte at position ${i} doesn't match`
        );
      }
    }).then(common.mustCall());
}

// Test with AES-GCM using TypedArray view of cross-realm ArrayBuffer
{
  const context = vm.createContext({});
  const crossRealmBuffer = vm.runInContext('new ArrayBuffer(16)', context);

  // Fill the buffer with some data
  const dataView = new Uint8Array(crossRealmBuffer);
  for (let i = 0; i < dataView.length; i++) {
    dataView[i] = i % 256;
  }

  // Generate a key
  subtle.generateKey({
    name: 'AES-GCM',
    length: 256
  }, true, ['encrypt', 'decrypt'])
    .then(async (key) => {
      // Create an initialization vector
      const iv = crypto.getRandomValues(new Uint8Array(12));

      // Encrypt using the TypedArray view of cross-realm ArrayBuffer
      const ciphertext = await subtle.encrypt(
        { name: 'AES-GCM', iv },
        key,
        dataView
      );
        // Decrypt
      const plaintext = await subtle.decrypt(
        { name: 'AES-GCM', iv },
        key,
        ciphertext
      );

      // Verify the decrypted content matches original
      const decryptedView = new Uint8Array(plaintext);
      for (let i = 0; i < dataView.length; i++) {
        assert.strictEqual(
          decryptedView[i],
          dataView[i],
          `Byte at position ${i} doesn't match`
        );
      }
    }).then(common.mustCall());
}
