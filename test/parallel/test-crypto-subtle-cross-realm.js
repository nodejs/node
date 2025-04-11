'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = crypto;
const vm = require('vm');

// Test with same-realm ArrayBuffer
{
  const samerealmData = new Uint8Array([1, 2, 3, 4]).buffer;

  subtle.digest('SHA-256', samerealmData)
    .then((result) => {
      assert(result instanceof ArrayBuffer);
      assert.strictEqual(result.byteLength, 32); // SHA-256 is 32 bytes
    })
    .catch(common.mustNotCall());
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
    .then((result) => {
      assert(result instanceof ArrayBuffer);
      assert.strictEqual(result.byteLength, 32); // SHA-256 is 32 bytes
    })
    .catch(common.mustNotCall('Should not reject cross-realm ArrayBuffer'));
}

// Test with both TypedArray buffer methods
{
  const context = vm.createContext({});
  const crossrealmUint8Array = vm.runInContext('new Uint8Array([1, 2, 3, 4])', context);

  // Test the .buffer property (this was failing before)
  subtle.digest('SHA-256', crossrealmUint8Array.buffer)
    .then((result) => {
      assert(result instanceof ArrayBuffer);
      assert.strictEqual(result.byteLength, 32);
    })
    .catch(common.mustNotCall('Should not reject TypedArray.buffer'));

  // Test passing the TypedArray directly (should work both before and after the fix)
  subtle.digest('SHA-256', crossrealmUint8Array)
    .then((result) => {
      assert(result instanceof ArrayBuffer);
      assert.strictEqual(result.byteLength, 32);
    })
    .catch(common.mustNotCall('Should not reject TypedArray'));
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
    .then((key) => {
      // Create an initialization vector
      const iv = crypto.getRandomValues(new Uint8Array(12));

      // Encrypt using the cross-realm ArrayBuffer
      return subtle.encrypt(
        { name: 'AES-GCM', iv },
        key,
        crossRealmBuffer
      ).then((ciphertext) => {
        // Decrypt
        return subtle.decrypt(
          { name: 'AES-GCM', iv },
          key,
          ciphertext
        );
      }).then((plaintext) => {
        // Verify the decrypted content matches original
        const decryptedView = new Uint8Array(plaintext);
        for (let i = 0; i < dataView.length; i++) {
          assert.strictEqual(
            decryptedView[i],
            dataView[i],
            `Byte at position ${i} doesn't match`
          );
        }
      });
    })
    .catch(common.mustNotCall('Should not reject AES-GCM with cross-realm ArrayBuffer'));
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
    .then((key) => {
      // Create an initialization vector
      const iv = crypto.getRandomValues(new Uint8Array(12));

      // Encrypt using the TypedArray view of cross-realm ArrayBuffer
      return subtle.encrypt(
        { name: 'AES-GCM', iv },
        key,
        dataView
      ).then((ciphertext) => {
        // Decrypt
        return subtle.decrypt(
          { name: 'AES-GCM', iv },
          key,
          ciphertext
        );
      }).then((plaintext) => {
        // Verify the decrypted content matches original
        const decryptedView = new Uint8Array(plaintext);
        for (let i = 0; i < dataView.length; i++) {
          assert.strictEqual(
            decryptedView[i],
            dataView[i],
            `Byte at position ${i} doesn't match`
          );
        }
      });
    })
    .catch(common.mustNotCall('Should not reject AES-GCM with TypedArray view'));
}
