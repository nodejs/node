'use strict';
// Flags: --expose-internals
const common = require('../common');

if (!common.hasIntl)
  common.skip('missing Intl');

const { internalBinding } = require('internal/test/binding');
const icu = internalBinding('icu');
const assert = require('assert');

// Test hasConverter method
assert(icu.hasConverter('utf-8'),
       'hasConverter should report coverter exists for utf-8');
assert(!icu.hasConverter('x'),
       'hasConverter should report coverter does not exist for x');

const tests = require('../fixtures/url-idna.js');
const fixtures = require('../common/fixtures');
const wptToASCIITests = require(
  fixtures.path('wpt', 'url', 'resources', 'toascii.json')
);

{
  for (const [i, { ascii, unicode }] of tests.entries()) {
    assert.strictEqual(ascii, icu.toASCII(unicode), `toASCII(${i + 1})`);
    assert.strictEqual(unicode, icu.toUnicode(ascii), `toUnicode(${i + 1})`);
    assert.strictEqual(ascii, icu.toASCII(icu.toUnicode(ascii)),
                       `toASCII(toUnicode(${i + 1}))`);
    assert.strictEqual(unicode, icu.toUnicode(icu.toASCII(unicode)),
                       `toUnicode(toASCII(${i + 1}))`);
  }
}

{
  for (const [i, test] of wptToASCIITests.entries()) {
    if (typeof test === 'string')
      continue; // skip comments
    const { comment, input, output } = test;
    let caseComment = `case ${i + 1}`;
    if (comment)
      caseComment += ` (${comment})`;
    if (output === null) {
      assert.throws(
        () => icu.toASCII(input),
        {
          code: 'ERR_INVALID_ARG_VALUE',
          name: 'TypeError',
          message: 'Cannot convert name to ASCII'
        }
      );
      icu.toASCII(input, true); // Should not throw.
    } else {
      assert.strictEqual(icu.toASCII(input), output, `ToASCII ${caseComment}`);
      assert.strictEqual(icu.toASCII(input, true), output,
                         `ToASCII ${caseComment} in lenient mode`);
    }
    icu.toUnicode(input); // Should not throw.
  }
}
