/* eslint-disable no-proto */
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
// Flags: --expose_internals
const internalTLS = require('internal/tls');
const tls = require('tls');

const noOutput = common.mustNotCall();
common.hijackStderr(noOutput);

{
  const singles = 'C=US\nST=CA\nL=SF\nO=Node.js Foundation\nOU=Node.js\n' +
                  'CN=ca1\nemailAddress=ry@clouds.org';
  const singlesOut = internalTLS.parseCertString(singles);
  assert.deepStrictEqual(singlesOut, {
    __proto__: null,
    C: 'US',
    ST: 'CA',
    L: 'SF',
    O: 'Node.js Foundation',
    OU: 'Node.js',
    CN: 'ca1',
    emailAddress: 'ry@clouds.org'
  });
}

{
  const doubles = 'OU=Domain Control Validated\nOU=PositiveSSL Wildcard\n' +
                  'CN=*.nodejs.org';
  const doublesOut = internalTLS.parseCertString(doubles);
  assert.deepStrictEqual(doublesOut, {
    __proto__: null,
    OU: [ 'Domain Control Validated', 'PositiveSSL Wildcard' ],
    CN: '*.nodejs.org'
  });
}

{
  const invalid = 'fhqwhgads';
  const invalidOut = internalTLS.parseCertString(invalid);
  assert.deepStrictEqual(invalidOut, { __proto__: null });
}

{
  const input = '__proto__=mostly harmless\nhasOwnProperty=not a function';
  const expected = Object.create(null);
  expected.__proto__ = 'mostly harmless';
  expected.hasOwnProperty = 'not a function';
  assert.deepStrictEqual(internalTLS.parseCertString(input), expected);
}

common.restoreStderr();

{
  common.expectWarning('DeprecationWarning',
                       'tls.parseCertString() is deprecated. ' +
                       'Please use querystring.parse() instead.');

  const ret = tls.parseCertString('foo=bar');
  assert.deepStrictEqual(ret, { __proto__: null, foo: 'bar' });
}
