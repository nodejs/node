'use strict';

const {
  Array,
  NumberPrototypeToString,
  StringPrototypePadStart,
  Uint8Array,
} = primordials;

const { randomFillSync } = require('internal/crypto/random');

// Pre-allocate a 4KB (page-aligned) buffer of random bytes. Refill when
// exhausted. This amortizes the cost of random number generation across
// many ID creations. Each trace ID uses 16 bytes, each span ID 8 bytes,
// so 4096 bytes provides ~170 spans before refill.
const kBufferSize = 4096;
const randomBuffer = new Uint8Array(kBufferSize);
let randomOffset = kBufferSize; // Start at end to trigger first fill

// Hex lookup table for fast byte-to-hex conversion.
const hexTable = new Array(256);
for (let i = 0; i < 256; i++) {
  hexTable[i] = StringPrototypePadStart(
    NumberPrototypeToString(i, 16), 2, '0',
  );
}

function ensureRandomBytes(needed) {
  if (randomOffset + needed > kBufferSize) {
    randomFillSync(randomBuffer);
    randomOffset = 0;
  }
}

function generateTraceId() {
  ensureRandomBytes(16);
  let id = '';
  for (let i = 0; i < 16; i++) {
    id += hexTable[randomBuffer[randomOffset++]];
  }
  return id;
}

function generateSpanId() {
  ensureRandomBytes(8);
  let id = '';
  for (let i = 0; i < 8; i++) {
    id += hexTable[randomBuffer[randomOffset++]];
  }
  return id;
}

module.exports = {
  generateTraceId,
  generateSpanId,
};
