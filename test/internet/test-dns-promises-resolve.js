'use strict';
require('../common');
const assert = require('assert');

const dnsPromises = require('dns').promises;

// Error when rrtype is invalid.
{
  const rrtype = 'DUMMY';
  assert.throws(
    () => dnsPromises.resolve('example.org', rrtype),
    {
      code: 'ERR_INVALID_OPT_VALUE',
      name: 'TypeError',
      message: `The value "${rrtype}" is invalid for option "rrtype"`
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
      message: 'The "rrtype" argument must be of type string. ' +
               `Received type ${typeof rrtype} (${rrtype})`
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
  })();
}
