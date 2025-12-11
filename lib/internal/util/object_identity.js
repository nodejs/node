'use strict';

/**
 * Object identity utility using xxHash32 for content-based equality.
 *
 * xxHash32 algorithm based on the official specification:
 * https://github.com/Cyan4973/xxHash/blob/dev/doc/xxhash_spec.md
 *
 * This is a pure JavaScript implementation suitable for Node.js internals.
 * No external dependencies required.
 *
 * Performance characteristics:
 * - Empty object: O(1)
 * - 1 key: O(1)
 * - 2-3 keys: O(k) with inline sort
 * - 4+ keys: O(k log k) with sorted keys cache
 *
 * Collision probability at 2000 items (default cardinality limit): ~0.05%
 */

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
  ArrayPrototypeSplice,
  MathImul,
  ObjectKeys,
  SafeMap,
  String,
  StringPrototypeCharCodeAt,
} = primordials;

const { kEmptyObject } = require('internal/util');

// xxHash32 prime constants from specification
const PRIME32_1 = 0x9E3779B1;
const PRIME32_2 = 0x85EBCA77;
const PRIME32_3 = 0xC2B2AE3D;
const PRIME32_5 = 0x165667B1;

// Default seed
const SEED = 0;

/**
 * 32-bit rotate left operation.
 * @param {number} value - 32-bit unsigned integer
 * @param {number} amount - Rotation amount (0-31)
 * @returns {number} Rotated value
 */
function rotl32(value, amount) {
  return ((value << amount) | (value >>> (32 - amount))) >>> 0;
}

/**
 * xxHash32 implementation for strings.
 * Optimized for attribute key-value pairs (typically ASCII/UTF-8 strings).
 * @param {string} str - String to hash
 * @param {number} seed - Hash seed (default: 0)
 * @returns {number} 32-bit hash value
 */
function xxHash32(str, seed = SEED) {
  const len = str.length;
  let h32;
  let index = 0;

  if (len >= 16) {
    // For longer strings, use the full algorithm with accumulators
    let v1 = (seed + PRIME32_1 + PRIME32_2) >>> 0;
    let v2 = (seed + PRIME32_2) >>> 0;
    let v3 = (seed + 0) >>> 0;
    let v4 = (seed - PRIME32_1) >>> 0;

    // Process 16-byte blocks
    const limit = len - 16;
    while (index <= limit) {
      // Read 4 bytes (1 char = 2 bytes in UTF-16, but we treat as byte stream)
      // For attribute strings (typically ASCII), this is efficient
      const k1 = (StringPrototypeCharCodeAt(str, index) |
                 (StringPrototypeCharCodeAt(str, index + 1) << 8) |
                 (StringPrototypeCharCodeAt(str, index + 2) << 16) |
                 (StringPrototypeCharCodeAt(str, index + 3) << 24)) >>> 0;
      index += 4;

      v1 = (v1 + MathImul(k1, PRIME32_2)) >>> 0;
      v1 = rotl32(v1, 13);
      v1 = MathImul(v1, PRIME32_1) >>> 0;

      const k2 = (StringPrototypeCharCodeAt(str, index) |
                 (StringPrototypeCharCodeAt(str, index + 1) << 8) |
                 (StringPrototypeCharCodeAt(str, index + 2) << 16) |
                 (StringPrototypeCharCodeAt(str, index + 3) << 24)) >>> 0;
      index += 4;

      v2 = (v2 + MathImul(k2, PRIME32_2)) >>> 0;
      v2 = rotl32(v2, 13);
      v2 = MathImul(v2, PRIME32_1) >>> 0;

      const k3 = (StringPrototypeCharCodeAt(str, index) |
                 (StringPrototypeCharCodeAt(str, index + 1) << 8) |
                 (StringPrototypeCharCodeAt(str, index + 2) << 16) |
                 (StringPrototypeCharCodeAt(str, index + 3) << 24)) >>> 0;
      index += 4;

      v3 = (v3 + MathImul(k3, PRIME32_2)) >>> 0;
      v3 = rotl32(v3, 13);
      v3 = MathImul(v3, PRIME32_1) >>> 0;

      const k4 = (StringPrototypeCharCodeAt(str, index) |
                 (StringPrototypeCharCodeAt(str, index + 1) << 8) |
                 (StringPrototypeCharCodeAt(str, index + 2) << 16) |
                 (StringPrototypeCharCodeAt(str, index + 3) << 24)) >>> 0;
      index += 4;

      v4 = (v4 + MathImul(k4, PRIME32_2)) >>> 0;
      v4 = rotl32(v4, 13);
      v4 = MathImul(v4, PRIME32_1) >>> 0;
    }

    // Merge accumulators
    h32 = rotl32(v1, 1) + rotl32(v2, 7) + rotl32(v3, 12) + rotl32(v4, 18);
    h32 = h32 >>> 0;
  } else {
    // Short string path
    h32 = (seed + PRIME32_5) >>> 0;
  }

  h32 = (h32 + len) >>> 0;

  // Process remaining bytes
  while (index < len) {
    const k1 = StringPrototypeCharCodeAt(str, index);
    index++;

    h32 = (h32 + MathImul(k1, PRIME32_5)) >>> 0;
    h32 = rotl32(h32, 11);
    h32 = MathImul(h32, PRIME32_1) >>> 0;
  }

  // Final avalanche mixing
  h32 ^= h32 >>> 15;
  h32 = MathImul(h32, PRIME32_2) >>> 0;
  h32 ^= h32 >>> 13;
  h32 = MathImul(h32, PRIME32_3) >>> 0;
  h32 ^= h32 >>> 16;

  return h32 >>> 0;
}

/**
 * Get sorted keys for an object, using cache when possible.
 * @param {Array<string>} keys - Unsorted keys
 * @param {Map} cache - Sorted keys cache
 * @param {Array} cacheOrder - LRU order tracking
 * @param {number} maxCacheSize - Maximum cache size
 * @returns {Array<string>} Sorted keys
 */
function getSortedKeys(keys, cache, cacheOrder, maxCacheSize) {
  const len = keys.length;

  // Inline sort for 2-3 keys (faster than array sort overhead)
  if (len === 2) {
    return keys[0] < keys[1] ? keys : [keys[1], keys[0]];
  }
  if (len === 3) {
    // Inline 3-element sort network
    let a = keys[0]; let b = keys[1]; let c = keys[2];
    if (a > b) ({ 0: a, 1: b } = [b, a]);
    if (b > c) ({ 0: b, 1: c } = [c, b]);
    if (a > b) ({ 0: a, 1: b } = [b, a]);
    return [a, b, c];
  }

  // For 4+ keys, use cache
  const shape = keys.join(',');
  let sorted = cache.get(shape);

  if (sorted !== undefined) {
    // Cache hit - update LRU order
    const idx = cacheOrder.indexOf(shape);
    if (idx !== -1) {
      // Move to end (most recently used)
      ArrayPrototypeSplice(cacheOrder, idx, 1);
      ArrayPrototypePush(cacheOrder, shape);
    }
    return sorted;
  }

  // Cache miss - sort and store
  sorted = [...keys].sort();

  // Evict least recently used if at capacity
  if (cache.size >= maxCacheSize) {
    const evictShape = ArrayPrototypeShift(cacheOrder);
    cache.delete(evictShape);
  }

  cache.set(shape, sorted);
  ArrayPrototypePush(cacheOrder, shape);

  return sorted;
}

/**
 * ObjectIdentity provides deterministic numeric hashing for plain objects.
 * Uses xxHash32 algorithm following the industry standard approach used by
 * OpenTelemetry and Prometheus.
 *
 * Each instance maintains an isolated LRU cache for sorted keys arrays.
 */
class ObjectIdentity {
  #sortedKeysCache;
  #sortedKeysCacheOrder;
  #maxSortedKeysCacheSize;

  /**
   * Create a new ObjectIdentity instance with configurable cache size.
   * @param {object} [options]
   * @param {number} [options.sortedKeysCacheSize=1000] - Max sorted keys cache entries
   */
  constructor(options = kEmptyObject) {
    this.#sortedKeysCache = new SafeMap();
    this.#sortedKeysCacheOrder = [];
    this.#maxSortedKeysCacheSize = options.sortedKeysCacheSize ?? 1000;
  }

  /**
   * Get a numeric hash for an object based on its content.
   *
   * Objects with the same key-value pairs produce the same hash:
   * getId({ a: 1, b: 2 }) === getId({ b: 2, a: 1 })
   *
   * Uses xxHash32 algorithm for fast, well-distributed hashing.
   * Hash collisions are possible but extremely rare at typical cardinalities
   * (~0.05% at 2000 unique attribute combinations).
   * @param {object} obj - Object to hash
   * @returns {number} 32-bit unsigned integer hash (0 for empty object)
   * @example
   * const oid = new ObjectIdentity();
   * const hash1 = oid.getId({ method: 'GET', status: 200 });
   * const hash2 = oid.getId({ status: 200, method: 'GET' });
   * // hash1 === hash2 (order doesn't matter)
   */
  getId(obj) {
    // Fast path: empty object constant
    if (obj === kEmptyObject) {
      return 0;
    }

    const keys = ObjectKeys(obj);
    const len = keys.length;

    if (len === 0) {
      return 0;
    }

    // For single key, hash directly (no sorting needed)
    if (len === 1) {
      const key = keys[0];
      const value = String(obj[key]);
      // Hash: key + separator + value
      return xxHash32(key + '\x00' + value);
    }

    // Multiple keys: sort for canonical order, then hash
    const sortedKeys = getSortedKeys(
      keys,
      this.#sortedKeysCache,
      this.#sortedKeysCacheOrder,
      this.#maxSortedKeysCacheSize,
    );

    // Build canonical string representation and hash it
    // Format: key1\x00value1\x00key2\x00value2...
    let canonical = sortedKeys[0] + '\x00' + String(obj[sortedKeys[0]]);
    for (let i = 1; i < len; i++) {
      const key = sortedKeys[i];
      canonical += '\x00' + key + '\x00' + String(obj[key]);
    }

    return xxHash32(canonical);
  }

  /**
   * Clear sorted keys cache. Useful for testing or memory reclamation.
   */
  clearCache() {
    this.#sortedKeysCache.clear();
    this.#sortedKeysCacheOrder.length = 0;
  }

  /**
   * Get current cache statistics.
   * @returns {object} Cache stats
   */
  getCacheStats() {
    return {
      size: this.#sortedKeysCache.size,
      max: this.#maxSortedKeysCacheSize,
    };
  }
}

module.exports = {
  ObjectIdentity,
};
