'use strict';
const common = require('../common');

if (!common.hasIntl)
  common.skip('missing Intl');

const assert = require('assert');
const { domainToASCII, domainToUnicode } = require('url');

// Tests below are not from WPT.
const tests = require('../fixtures/url-idna.js');
const wptToASCIITests = require('../fixtures/url-toascii.js');

{
  assert.throws(() => domainToASCII(), /^TypeError: The "domain" argument must be specified$/);
  assert.throws(() => domainToUnicode(), /^TypeError: The "domain" argument must be specified$/);
  assert.strictEqual(domainToASCII(undefined), 'undefined');
  assert.strictEqual(domainToUnicode(undefined), 'undefined');
}

{
  for (const [i, { ascii, unicode }] of tests.entries()) {
    assert.strictEqual(ascii, domainToASCII(unicode),
                       `domainToASCII(${i + 1})`);
    assert.strictEqual(unicode, domainToUnicode(ascii),
                       `domainToUnicode(${i + 1})`);
    assert.strictEqual(ascii, domainToASCII(domainToUnicode(ascii)),
                       `domainToASCII(domainToUnicode(${i + 1}))`);
    assert.strictEqual(unicode, domainToUnicode(domainToASCII(unicode)),
                       `domainToUnicode(domainToASCII(${i + 1}))`);
  }
}

{
  for (const [i, test] of wptToASCIITests.entries()) {
    if (typeof test === 'string')
      continue; // skip comments
    const { comment, input, output } = test;
    let caseComment = `Case ${i + 1}`;
    if (comment)
      caseComment += ` (${comment})`;
    if (output === null) {
      assert.strictEqual(domainToASCII(input), '', caseComment);
      assert.strictEqual(domainToUnicode(input), '', caseComment);
    } else {
      assert.strictEqual(domainToASCII(input), output, caseComment);
      const roundtripped = domainToASCII(domainToUnicode(input));
      assert.strictEqual(roundtripped, output, caseComment);
    }
  }
}
