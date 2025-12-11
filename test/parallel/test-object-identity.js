// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { ObjectIdentity } = require('internal/util/object_identity');

// Test 1: Empty object returns 0
{
  const oid = new ObjectIdentity();
  const hash = oid.getId({});
  assert.strictEqual(hash, 0);
}

// Test 2: Determinism - order doesn't matter
{
  const oid = new ObjectIdentity();
  const hash1 = oid.getId({ a: 1, b: 2 });
  const hash2 = oid.getId({ b: 2, a: 1 });
  assert.strictEqual(hash1, hash2);
}

// Test 3: Different values produce different hashes
{
  const oid = new ObjectIdentity();
  const hash1 = oid.getId({ a: 1, b: 2 });
  const hash2 = oid.getId({ a: 1, b: 3 });
  assert.notStrictEqual(hash1, hash2);
}

// Test 4: Single key
{
  const oid = new ObjectIdentity();
  const hash = oid.getId({ method: 'GET' });
  assert.strictEqual(typeof hash, 'number');
  assert(hash > 0);
}

// Test 5: Multiple keys with different orders
{
  const oid = new ObjectIdentity();
  const hash1 = oid.getId({ method: 'GET', status: 200, path: '/api' });
  const hash2 = oid.getId({ status: 200, path: '/api', method: 'GET' });
  const hash3 = oid.getId({ path: '/api', method: 'GET', status: 200 });
  assert.strictEqual(hash1, hash2);
  assert.strictEqual(hash2, hash3);
}

// Test 6: Hash is always a 32-bit unsigned integer
{
  const oid = new ObjectIdentity();
  const hash = oid.getId({ a: 'test', b: 123, c: true });
  assert.strictEqual(typeof hash, 'number');
  assert(hash >= 0);
  assert(hash <= 0xFFFFFFFF);
  assert.strictEqual(hash, hash >>> 0); // Verify it's a 32-bit uint
}

// Test 7: Same content produces same hash across instances
{
  const oid1 = new ObjectIdentity();
  const oid2 = new ObjectIdentity();
  const attrs = { method: 'POST', status: 201 };
  assert.strictEqual(oid1.getId(attrs), oid2.getId(attrs));
}

// Test 8: Sorted keys cache
{
  const oid = new ObjectIdentity({ sortedKeysCacheSize: 10 });

  // First call - cache miss
  const hash1 = oid.getId({ d: 4, c: 3, b: 2, a: 1 });
  const stats1 = oid.getCacheStats();
  assert.strictEqual(stats1.size, 1);

  // Second call with same key ORDER - cache hit
  const hash2 = oid.getId({ d: 4, c: 3, b: 2, a: 1 });
  assert.strictEqual(hash1, hash2);
  const stats2 = oid.getCacheStats();
  assert.strictEqual(stats2.size, 1); // Same order = cache hit

  // Third call with different key ORDER - new cache entry but same hash
  const hash3 = oid.getId({ a: 1, b: 2, c: 3, d: 4 });
  assert.strictEqual(hash1, hash3); // Same content = same hash
  const stats3 = oid.getCacheStats();
  assert.strictEqual(stats3.size, 2); // Different order = new cache entry
}

// Test 9: Cache size limit and LRU eviction
{
  const oid = new ObjectIdentity({ sortedKeysCacheSize: 3 });

  // Fill cache to capacity
  oid.getId({ a: 1, b: 2, c: 3, d: 4 });
  oid.getId({ e: 5, f: 6, g: 7, h: 8 });
  oid.getId({ i: 9, j: 10, k: 11, l: 12 });

  let stats = oid.getCacheStats();
  assert.strictEqual(stats.size, 3);
  assert.strictEqual(stats.max, 3);

  // Add one more - should evict least recently used
  oid.getId({ m: 13, n: 14, o: 15, p: 16 });
  stats = oid.getCacheStats();
  assert.strictEqual(stats.size, 3);
}

// Test 10: Clear cache
{
  const oid = new ObjectIdentity();
  oid.getId({ a: 1, b: 2, c: 3, d: 4 });

  let stats = oid.getCacheStats();
  assert.strictEqual(stats.size, 1);

  oid.clearCache();
  stats = oid.getCacheStats();
  assert.strictEqual(stats.size, 0);
}

// Test 11: Inline sort optimization for 2-3 keys
{
  const oid = new ObjectIdentity();

  // 2 keys - should use inline sort
  const hash2a = oid.getId({ b: 2, a: 1 });
  const hash2b = oid.getId({ a: 1, b: 2 });
  assert.strictEqual(hash2a, hash2b);

  // 3 keys - should use inline sort network
  const hash3a = oid.getId({ c: 3, a: 1, b: 2 });
  const hash3b = oid.getId({ b: 2, c: 3, a: 1 });
  assert.strictEqual(hash3a, hash3b);
}

// Test 12: String values are handled correctly
{
  const oid = new ObjectIdentity();
  const hash1 = oid.getId({ method: 'GET', path: '/api/users' });
  const hash2 = oid.getId({ path: '/api/users', method: 'GET' });
  assert.strictEqual(hash1, hash2);

  // Different string values should produce different hashes
  const hash3 = oid.getId({ method: 'POST', path: '/api/users' });
  assert.notStrictEqual(hash1, hash3);
}

// Test 13: Numeric values are handled correctly
{
  const oid = new ObjectIdentity();
  const hash1 = oid.getId({ status: 200 });
  const hash2 = oid.getId({ status: 404 });
  assert.notStrictEqual(hash1, hash2);
}

// Test 14: Boolean values are handled correctly
{
  const oid = new ObjectIdentity();
  const hash1 = oid.getId({ success: true });
  const hash2 = oid.getId({ success: false });
  assert.notStrictEqual(hash1, hash2);
}

// Test 15: Realistic metric attributes
{
  const oid = new ObjectIdentity();

  // Typical HTTP request attributes
  const attrs1 = {
    method: 'GET',
    status: 200,
    path: '/api/users',
    host: 'example.com',
  };

  const attrs2 = {
    host: 'example.com',
    path: '/api/users',
    status: 200,
    method: 'GET',
  };

  const hash1 = oid.getId(attrs1);
  const hash2 = oid.getId(attrs2);
  assert.strictEqual(hash1, hash2);

  // Different attributes should produce different hash
  const attrs3 = { ...attrs1, status: 404 };
  const hash3 = oid.getId(attrs3);
  assert.notStrictEqual(hash1, hash3);
}

// Test 16: Hash distribution (no obvious collisions in small set)
{
  const oid = new ObjectIdentity();
  const hashes = new Set();

  // Generate 100 different attribute combinations
  for (let i = 0; i < 100; i++) {
    const hash = oid.getId({
      method: ['GET', 'POST', 'PUT'][i % 3],
      status: 200 + (i % 5),
      id: i,
    });
    hashes.add(hash);
  }

  // Should have close to 100 unique hashes (allowing for rare collisions)
  assert(hashes.size >= 95, `Expected >= 95 unique hashes, got ${hashes.size}`);
}

// Test 17: kEmptyObject constant optimization
{
  const { kEmptyObject } = require('internal/util');
  const oid = new ObjectIdentity();
  const hash = oid.getId(kEmptyObject);
  assert.strictEqual(hash, 0);
}
