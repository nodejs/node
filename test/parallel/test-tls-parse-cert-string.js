/* eslint-disable no-proto */
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

{
  const singles = 'C=US\nST=CA\nL=SF\nO=Node.js Foundation\nOU=Node.js\n' +
                  'CN=ca1\nemailAddress=ry@clouds.org';
  const singlesOut = tls.parseCertString(singles);
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
  const doublesOut = tls.parseCertString(doubles);
  assert.deepStrictEqual(doublesOut, {
    __proto__: null,
    OU: [ 'Domain Control Validated', 'PositiveSSL Wildcard' ],
    CN: '*.nodejs.org'
  });
}

{
  const invalid = 'fhqwhgads';
  const invalidOut = tls.parseCertString(invalid);
  assert.deepStrictEqual(invalidOut, { __proto__: null });
}

{
  const input = '__proto__=mostly harmless\nhasOwnProperty=not a function';
  const expected = Object.create(null);
  expected.__proto__ = 'mostly harmless';
  expected.hasOwnProperty = 'not a function';
  assert.deepStrictEqual(tls.parseCertString(input), expected);
}
