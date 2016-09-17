'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const crypto = require('crypto');

const UTF_INPUT = 'öäü';
const TEST_CASES = [
  {
    options: undefined,
    // Hash of "öäü" in default format (utf8)
    output: '4f53d15bee524f082380e6d7247cc541e7cb0d10c64efdcc935ceeb1e7ea345c',
  },
  {
    options: {},
    // Hash of "öäü" in default format (utf8)
    output: '4f53d15bee524f082380e6d7247cc541e7cb0d10c64efdcc935ceeb1e7ea345c',
  },
  {
    options: {
      defaultEncoding: 'latin1',
    },
    // Hash of "öäü" in latin1 format
    output: 'cd37bccd5786e2e76d9b18c871e919e6eb11cc12d868f5ae41c40ccff8e44830',
  },
  {
    options: {
      defaultEncoding: 'utf8',
    },
    // Hash of "öäü" in utf8 format
    output: '4f53d15bee524f082380e6d7247cc541e7cb0d10c64efdcc935ceeb1e7ea345c',
  },
  {
    options: {
      defaultEncoding: 'ascii',
    },
    // Hash of "öäü" in ascii format
    output: 'cd37bccd5786e2e76d9b18c871e919e6eb11cc12d868f5ae41c40ccff8e44830',
  },
];

for (const test of TEST_CASES) {
  const hashValue = crypto.createHash('sha256')
    .update(UTF_INPUT, test.options && test.options.defaultEncoding)
    .digest('hex');

  assert.equal(hashValue, test.output);
}

for (const test of TEST_CASES) {
  const hash = crypto.createHash('sha256', test.options);
  let hashValue = '';

  hash.on('data', (data) => {
    hashValue += data.toString('hex');
  });

  hash.on('end', common.mustCall(() => {
    assert.equal(hashValue, test.output);
  }));

  hash.write(UTF_INPUT);
  hash.end();
}
