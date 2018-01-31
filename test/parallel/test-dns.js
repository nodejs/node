// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');

const dns = require('dns');

const existing = dns.getServers();
assert(existing.length > 0);

// Verify that setServers() handles arrays with holes and other oddities
assert.doesNotThrow(() => {
  const servers = [];

  servers[0] = '127.0.0.1';
  servers[2] = '0.0.0.0';
  dns.setServers(servers);

  assert.deepStrictEqual(dns.getServers(), ['127.0.0.1', '0.0.0.0']);
});

assert.doesNotThrow(() => {
  const servers = ['127.0.0.1', '192.168.1.1'];

  servers[3] = '127.1.0.1';
  servers[4] = '127.1.0.1';
  servers[5] = '127.1.1.1';

  Object.defineProperty(servers, 2, {
    enumerable: true,
    get: () => {
      servers.length = 3;
      return '0.0.0.0';
    }
  });

  dns.setServers(servers);
  assert.deepStrictEqual(dns.getServers(), [
    '127.0.0.1',
    '192.168.1.1',
    '0.0.0.0'
  ]);
});

const goog = [
  '8.8.8.8',
  '8.8.4.4',
];
assert.doesNotThrow(() => dns.setServers(goog));
assert.deepStrictEqual(dns.getServers(), goog);
common.expectsError(() => dns.setServers(['foobar']), {
  code: 'ERR_INVALID_IP_ADDRESS',
  type: Error,
  message: 'Invalid IP address: foobar'
});
common.expectsError(() => dns.setServers(['127.0.0.1:va']), {
  code: 'ERR_INVALID_IP_ADDRESS',
  type: Error,
  message: 'Invalid IP address: 127.0.0.1:va'
});
assert.deepStrictEqual(dns.getServers(), goog);

const goog6 = [
  '2001:4860:4860::8888',
  '2001:4860:4860::8844',
];
assert.doesNotThrow(() => dns.setServers(goog6));
assert.deepStrictEqual(dns.getServers(), goog6);

goog6.push('4.4.4.4');
dns.setServers(goog6);
assert.deepStrictEqual(dns.getServers(), goog6);

const ports = [
  '4.4.4.4:53',
  '[2001:4860:4860::8888]:53',
  '103.238.225.181:666',
  '[fe80::483a:5aff:fee6:1f04]:666'
];
const portsExpected = [
  '4.4.4.4',
  '2001:4860:4860::8888',
  '103.238.225.181:666',
  '[fe80::483a:5aff:fee6:1f04]:666'
];
dns.setServers(ports);
assert.deepStrictEqual(dns.getServers(), portsExpected);

assert.doesNotThrow(() => dns.setServers([]));
assert.deepStrictEqual(dns.getServers(), []);

common.expectsError(() => {
  dns.resolve('example.com', [], common.mustNotCall());
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "rrtype" argument must be of type string. ' +
           'Received type object'
});

// dns.lookup should accept only falsey and string values
{
  const errorReg = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "hostname" argument must be one of type string or falsy/
  }, 5);

  assert.throws(() => dns.lookup({}, common.mustNotCall()), errorReg);

  assert.throws(() => dns.lookup([], common.mustNotCall()), errorReg);

  assert.throws(() => dns.lookup(true, common.mustNotCall()), errorReg);

  assert.throws(() => dns.lookup(1, common.mustNotCall()), errorReg);

  assert.throws(() => dns.lookup(common.mustNotCall(), common.mustNotCall()),
                errorReg);
}

// dns.lookup should accept falsey values
{
  const checkCallback = (err, address, family) => {
    assert.ifError(err);
    assert.strictEqual(address, null);
    assert.strictEqual(family, 4);
  };

  assert.doesNotThrow(() => dns.lookup('', common.mustCall(checkCallback)));

  assert.doesNotThrow(() => dns.lookup(null, common.mustCall(checkCallback)));

  assert.doesNotThrow(() => dns.lookup(undefined,
                                       common.mustCall(checkCallback)));

  assert.doesNotThrow(() => dns.lookup(0, common.mustCall(checkCallback)));

  assert.doesNotThrow(() => dns.lookup(NaN, common.mustCall(checkCallback)));
}

/*
 * Make sure that dns.lookup throws if hints does not represent a valid flag.
 * (dns.V4MAPPED | dns.ADDRCONFIG) + 1 is invalid because:
 * - it's different from dns.V4MAPPED and dns.ADDRCONFIG.
 * - it's different from them bitwise ored.
 * - it's different from 0.
 * - it's an odd number different than 1, and thus is invalid, because
 * flags are either === 1 or even.
 */
common.expectsError(() => {
  dns.lookup('nodejs.org', { hints: (dns.V4MAPPED | dns.ADDRCONFIG) + 1 },
             common.mustNotCall());
}, {
  code: 'ERR_INVALID_OPT_VALUE',
  type: TypeError,
  message: /The value "\d+" is invalid for option "hints"/
});

common.expectsError(() => dns.lookup('nodejs.org'), {
  code: 'ERR_INVALID_CALLBACK',
  type: TypeError
});

common.expectsError(() => dns.lookup('nodejs.org', 4), {
  code: 'ERR_INVALID_CALLBACK',
  type: TypeError
});

assert.doesNotThrow(() => dns.lookup('', { family: 4, hints: 0 },
                                     common.mustCall()));

assert.doesNotThrow(() => {
  dns.lookup('', {
    family: 6,
    hints: dns.ADDRCONFIG
  }, common.mustCall());
});

assert.doesNotThrow(() => dns.lookup('', { hints: dns.V4MAPPED },
                                     common.mustCall()));

assert.doesNotThrow(() => {
  dns.lookup('', {
    hints: dns.ADDRCONFIG | dns.V4MAPPED
  }, common.mustCall());
});

common.expectsError(() => dns.lookupService('0.0.0.0'), {
  code: 'ERR_MISSING_ARGS',
  type: TypeError,
  message: 'The "host", "port", and "callback" arguments must be specified'
});

const invalidHost = 'fasdfdsaf';
common.expectsError(() => {
  dns.lookupService(invalidHost, 0, common.mustNotCall());
}, {
  code: 'ERR_INVALID_OPT_VALUE',
  type: TypeError,
  message: `The value "${invalidHost}" is invalid for option "host"`
});

const portErr = (port) => {
  common.expectsError(
    () => {
      dns.lookupService('0.0.0.0', port, common.mustNotCall());
    },
    {
      code: 'ERR_SOCKET_BAD_PORT',
      message:
        `Port should be > 0 and < 65536. Received ${port}.`,
      type: RangeError
    }
  );
};
portErr(null);
portErr(undefined);
portErr(65538);
portErr('test');

common.expectsError(() => {
  dns.lookupService('0.0.0.0', 80, null);
}, {
  code: 'ERR_INVALID_CALLBACK',
  type: TypeError
});
