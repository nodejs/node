'use strict';
const common = require('../common');
const assert = require('assert');

const dnsPromises = require('dns').promises;

// Error when rrtype is invalid.
{
  const rrtype = 'DUMMY';
  assert.throws(
    () => dnsPromises.resolve('example.org', rrtype),
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
    }
  );
}

// Error when rrtype is a number.
{
  const rrtype = 0;
  assert.throws(
    () => dnsPromises.resolve('example.org', rrtype),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
}

// Setting rrtype to undefined should work like resolve4.
{
  (async function() {
    const rrtype = undefined;
    const result = await dnsPromises.resolve('example.org', rrtype);
    assert.ok(result !== undefined);
    assert.ok(result.length > 0);
  })().then(common.mustCall());
}
