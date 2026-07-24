'use strict';

const assert = require('assert');

// Test the encoding operations cache functionality indirectly
// This tests the performance optimization added to getEncodingOps through Buffer operations

const { Buffer } = require('buffer');

// Test valid encodings work consistently (cache should not affect correctness)
const validEncodings = [
  'utf8',
  'utf-8', 
  'ascii',
  'latin1',
  'binary',
  'base64',
  'base64url',
  'hex',
  'ucs2',
  'ucs-2',
  'utf16le',
  'utf-16le',
];

const invalidEncodings = [
  'utf9',
  'utf-7',
  'Unicode-FTW',
  'invalid',
  'unknown-encoding',
];

// Test that valid encodings work consistently through Buffer.byteLength
// (which uses getEncodingOps internally)
validEncodings.forEach((encoding) => {
  const testString = 'Hello, 世界! 🌍';
  
  // Multiple calls should return the same result (tests cache correctness)
  const length1 = Buffer.byteLength(testString, encoding);
  const length2 = Buffer.byteLength(testString, encoding);
  const length3 = Buffer.byteLength(testString, encoding);
  
  assert.strictEqual(length1, length2, `ByteLength should be consistent for ${encoding}`);
  assert.strictEqual(length2, length3, `ByteLength should be consistent across multiple calls for ${encoding}`);
  assert(length1 > 0, `Valid encoding ${encoding} should return positive byte length`);
  
  // Test Buffer.from consistency
  const buf1 = Buffer.from(testString, encoding);
  const buf2 = Buffer.from(testString, encoding);
  assert(buf1.equals(buf2), `Buffer.from should be consistent for ${encoding}`);
});

// Test that invalid encodings are handled consistently
invalidEncodings.forEach((encoding) => {
  let firstError, secondError;
  
  // Test first call
  try {
    Buffer.byteLength('test', encoding);
    firstError = null;
  } catch (err) {
    firstError = err.message;
  }
  
  // Test second call (should behave identically)
  try {
    Buffer.byteLength('test', encoding);
    secondError = null;
  } catch (err) {
    secondError = err.message;
  }
  
  // Both calls should behave the same way (cache consistency)
  assert.strictEqual(firstError, secondError, 
    `Invalid encoding ${encoding} should behave consistently across calls`);
});

// Test case sensitivity consistency (cache should handle this correctly)
const caseSensitiveTests = [
  ['utf8', 'UTF8'],
  ['ascii', 'ASCII'],
  ['hex', 'HEX'],
  ['base64', 'BASE64'],
];

caseSensitiveTests.forEach(([lower, upper]) => {
  const testString = 'test123';
  const lowerLength = Buffer.byteLength(testString, lower);
  const upperLength = Buffer.byteLength(testString, upper);
  
  // Both should work and return the same result
  assert.strictEqual(lowerLength, upperLength, 
    `Case variations ${lower}/${upper} should return same byte length`);
});

// Performance smoke test - rapid repeated calls shouldn't crash or behave inconsistently
const performanceTest = () => {
  const testString = 'Performance test string with émojis 🚀';
  const iterations = 10000;
  
  const startTime = process.hrtime.bigint();
  
  for (let i = 0; i < iterations; i++) {
    // Mix different encodings to test cache efficiency
    Buffer.byteLength(testString, 'utf8');
    Buffer.byteLength(testString, 'ascii');
    Buffer.byteLength(testString, 'base64');
    Buffer.byteLength(testString, 'hex');
  }
  
  const endTime = process.hrtime.bigint();
  const duration = Number(endTime - startTime) / 1000000; // Convert to milliseconds
  
  // Should complete in reasonable time (cache should make it fast)
  assert(duration < 1000, `Performance test should complete quickly, took ${duration}ms`);
};

performanceTest();

console.log('All encoding operations cache tests passed');