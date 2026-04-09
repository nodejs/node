'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  randomUUIDv7,
} = require('crypto');

{
  const uuid = randomUUIDv7();
  assert.strictEqual(typeof uuid, 'string');
  assert.strictEqual(uuid.length, 36);

  // UUIDv7 format: xxxxxxxx-xxxx-7xxx-[89ab]xxx-xxxxxxxxxxxx
  assert.match(
    uuid,
    /^[0-9a-f]{8}-[0-9a-f]{4}-7[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/,
  );
}

{
  const uuid = randomUUIDv7();

  assert.strictEqual(
    Buffer.from(uuid.slice(14, 16), 'hex')[0] & 0xf0, 0x70,
  );

  assert.strictEqual(
    Buffer.from(uuid.slice(19, 21), 'hex')[0] & 0b1100_0000, 0b1000_0000,
  );
}

{
  const seen = new Set();
  for (let i = 0; i < 1000; i++) {
    const uuid = randomUUIDv7();
    assert(!seen.has(uuid), `Duplicate UUID generated: ${uuid}`);
    seen.add(uuid);
  }
}

// Timestamp: the embedded timestamp should approximate Date.now().
{
  const before = Date.now();
  const uuid = randomUUIDv7();
  const after = Date.now();

  // Extract the 48-bit timestamp from the UUID.
  // Bytes 0-3 (chars 0-8) and bytes 4-5 (chars 9-13, skipping the dash).
  const hex = uuid.replace(/-/g, '');
  const timestampHex = hex.slice(0, 12); // first 48 bits = 12 hex chars
  const timestamp = parseInt(timestampHex, 16);

  assert(timestamp >= before, `Timestamp ${timestamp} < before ${before}`);
  // The monotonic counter may have overflowed in a prior call and advanced
  // v7LastTimestamp by 1 ms beyond wall-clock time (RFC 9562 §6.2 allows this).
  assert(timestamp <= after + 1, `Timestamp ${timestamp} > after+1 ${after + 1}`);
}

{
  let prev = randomUUIDv7();
  for (let i = 0; i < 100; i++) {
    const curr = randomUUIDv7();
    // With a monotonic counter in rand_a, each UUID must be strictly greater
    // than the previous regardless of whether the timestamp changed.
    assert(curr > prev,
           `UUID ordering violated: ${curr} <= ${prev}`);
    prev = curr;
  }
}

// Sub-millisecond ordering: a tight synchronous burst exercises the counter
// increment path and must also produce strictly increasing UUIDs.
{
  const burst = [];
  for (let i = 0; i < 500; i++) {
    burst.push(randomUUIDv7());
  }
  for (let i = 1; i < burst.length; i++) {
    assert(burst[i] > burst[i - 1],
           `Sub-millisecond ordering violated at index ${i}: ` +
           `${burst[i]} <= ${burst[i - 1]}`);
  }
}

// Ensure randomUUIDv7 takes no arguments (or ignores them gracefully).
{
  const uuid = randomUUIDv7();
  assert.match(
    uuid,
    /^[0-9a-f]{8}-[0-9a-f]{4}-7[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/,
  );
}

{
  const uuidv7Regex =
    /^[0-9a-f]{8}-[0-9a-f]{4}-7[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/;

  assert.match(randomUUIDv7({ disableEntropyCache: true }), uuidv7Regex);
  assert.match(randomUUIDv7({ disableEntropyCache: true }), uuidv7Regex);
  assert.match(randomUUIDv7({ disableEntropyCache: true }), uuidv7Regex);
  assert.match(randomUUIDv7({ disableEntropyCache: true }), uuidv7Regex);

  // monotonic: false — rand_a is random; UUIDs must still be valid but are not
  // guaranteed to be strictly ordered within the same millisecond.
  assert.match(randomUUIDv7({ monotonic: false }), uuidv7Regex);
  assert.match(randomUUIDv7({ monotonic: false, disableEntropyCache: true }), uuidv7Regex);

  assert.throws(() => randomUUIDv7(1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => randomUUIDv7({ disableEntropyCache: '' }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => randomUUIDv7({ monotonic: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

{
  for (let n = 0; n < 130; n++) {
    const uuid = randomUUIDv7();
    assert.match(
      uuid,
      /^[0-9a-f]{8}-[0-9a-f]{4}-7[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/,
    );
  }
}
