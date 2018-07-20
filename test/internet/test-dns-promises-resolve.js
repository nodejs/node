'use strict';
const common = require('../common');
const assert = require('assert');

const dnsPromises = require('dns').promises;

// Error when rrtype is invalid.
{
  const rrtype = 'DUMMY';
  common.expectsError(
    () => dnsPromises.resolve('example.org', rrtype),
    {
      code: 'ERR_INVALID_OPT_VALUE',
      type: TypeError,
      message: `The value "${rrtype}" is invalid for option "rrtype"`
    }
  );
}

// Error when rrtype is a number.
{
  const rrtype = 0;
  common.expectsError(
    () => dnsPromises.resolve('example.org', rrtype),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "rrtype" argument must be of type string. ' +
               `Received type ${typeof rrtype}`
    }
  );
}

// rrtype is undefined, it's same as resolve4
{
  (async function() {
    const rrtype = undefined;
    const result = await dnsPromises.resolve('example.org', rrtype);
    assert.ok(result !== undefined);
    assert.ok(result.length > 0);
  })();
}
