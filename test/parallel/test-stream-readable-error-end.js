'use strict';

const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');

{
  // Ensure 'end' is not emitted after 'error'.
  // This test is slightly more complicated than
  // needed in order to better illustrate the invariant.

  const r = new Readable({ read() {} });

  let errorEmitted = false;

  r.on('data', common.mustCall());
  r.on('end', () => {
    assert.strictEqual(!errorEmitted);
  });
  r.on('error', () => {
    errorEmitted = true;
  });
  r.push('asd');
  r.push(null);
  r.destroy(new Error('kaboom'));
}
