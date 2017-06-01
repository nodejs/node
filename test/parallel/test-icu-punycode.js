'use strict';
const common = require('../common');

if (!common.hasIntl)
  common.skip('missing Intl');

const icu = process.binding('icu');
const assert = require('assert');

const tests = require('../fixtures/url-idna.js');
const wptToASCIITests = require('../fixtures/url-toascii.js');

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
  const errMessage = /^Error: Cannot convert name to ASCII$/;

  for (const [i, test] of wptToASCIITests.entries()) {
    if (typeof test === 'string')
      continue; // skip comments
    const { comment, input, output } = test;
    let caseComment = `case ${i + 1}`;
    if (comment)
      caseComment += ` (${comment})`;
    if (output === null) {
      assert.throws(() => icu.toASCII(input),
                    errMessage, `ToASCII ${caseComment}`);
      assert.doesNotThrow(() => icu.toASCII(input, true),
                          `ToASCII ${caseComment} in lenient mode`);
    } else {
      assert.strictEqual(icu.toASCII(input), output, `ToASCII ${caseComment}`);
      assert.strictEqual(icu.toASCII(input, true), output,
                         `ToASCII ${caseComment} in lenient mode`);
    }
    assert.doesNotThrow(() => icu.toUnicode(input), `ToUnicode ${caseComment}`);
  }
}
