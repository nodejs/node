'use strict';

const common = require('../common');
if (!common.hasIntl)
  common.skip('missing Intl');

const assert = require('assert');
const url = require('url');
const URL = url.URL;

const myURL = new URL('http://xn--lck1c3crb1723bpq4a.com/a?a=b#c');

assert.strictEqual(
  url.format(myURL),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b#c'
);

assert.strictEqual(
  url.format(myURL, {}),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b#c'
);

{
  const expectedErr = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options" argument must be of type object'
  }, 4);
  assert.throws(() => url.format(myURL, true), expectedErr);
  assert.throws(() => url.format(myURL, 1), expectedErr);
  assert.throws(() => url.format(myURL, 'test'), expectedErr);
  assert.throws(() => url.format(myURL, Infinity), expectedErr);
}

// Any falsy value other than undefined will be treated as false.
// Any truthy value will be treated as true.

assert.strictEqual(
  url.format(myURL, { fragment: false }),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b'
);

assert.strictEqual(
  url.format(myURL, { fragment: '' }),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b'
);

assert.strictEqual(
  url.format(myURL, { fragment: 0 }),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b'
);

assert.strictEqual(
  url.format(myURL, { fragment: 1 }),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b#c'
);

assert.strictEqual(
  url.format(myURL, { fragment: {} }),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b#c'
);

assert.strictEqual(
  url.format(myURL, { search: false }),
  'http://xn--lck1c3crb1723bpq4a.com/a#c'
);

assert.strictEqual(
  url.format(myURL, { search: '' }),
  'http://xn--lck1c3crb1723bpq4a.com/a#c'
);

assert.strictEqual(
  url.format(myURL, { search: 0 }),
  'http://xn--lck1c3crb1723bpq4a.com/a#c'
);

assert.strictEqual(
  url.format(myURL, { search: 1 }),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b#c'
);

assert.strictEqual(
  url.format(myURL, { search: {} }),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b#c'
);

assert.strictEqual(
  url.format(myURL, { unicode: true }),
  'http://理容ナカムラ.com/a?a=b#c'
);

assert.strictEqual(
  url.format(myURL, { unicode: 1 }),
  'http://理容ナカムラ.com/a?a=b#c'
);

assert.strictEqual(
  url.format(myURL, { unicode: {} }),
  'http://理容ナカムラ.com/a?a=b#c'
);

assert.strictEqual(
  url.format(myURL, { unicode: false }),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b#c'
);

assert.strictEqual(
  url.format(myURL, { unicode: 0 }),
  'http://xn--lck1c3crb1723bpq4a.com/a?a=b#c'
);
