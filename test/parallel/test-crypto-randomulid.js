'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const {
  randomULID,
} = require('node:crypto');

function testMatch(ulid) {
  assert.match(
    ulid,
    /^[0-9A-HJKMNP-TV-Z]{26}$/
  );
}

const crockfordBase32 = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';

function incrementULID(ulid) {
  for (let i = ulid.length - 1; i >= 0; i--) {
    const index = crockfordBase32.indexOf(ulid[i]);
    if (index === -1) {
      throw new Error('Invalid ULID character');
    }
    if (index === crockfordBase32.length - 1) {
      ulid = ulid.substring(0, i) + '0' + ulid.substring(i + 1);
      continue;
    }

    ulid = ulid.substring(0, i) + crockfordBase32[index + 1] + ulid.substring(i + 1);
    return ulid;
  }
}

// Generate some ULIDs
for (let n = 0; n < 130; n++) {
  const ulid = randomULID();
  assert.strictEqual(typeof ulid, 'string');
  assert.strictEqual(ulid.length, 26);
  testMatch(ulid);
}

// Generate some ULIDs with a fixed seed
let last = randomULID(42);
for (let n = 0; n < 130; n++) {
  const ulid = randomULID(42);
  assert.strictEqual(ulid, incrementULID(last));
  last = ulid;
}

assert.throws(() => randomULID('foo'), {
  code: 'ERR_INVALID_ARG_TYPE'
});

assert.throws(() => randomULID(Math.PI), {
  code: 'ERR_INVALID_ARG_TYPE'
});

assert.throws(() => randomULID(-1), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => randomULID(NaN), {
  code: 'ERR_INVALID_ARG_TYPE'
});
