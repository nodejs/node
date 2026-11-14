'use strict';

// Tests below are not from WPT.

require('../common');
const assert = require('assert');

// Web IDL converts the assigned value to a string before the setter steps
// run, so the steps must see a URL that the conversion itself changed.
// Refs: https://github.com/nodejs/node/issues/67345
const setters = {
  protocol: ['http:', 'http://after.example/base'],
  username: ['user', 'https://user@after.example/base'],
  password: ['pass', 'https://:pass@after.example/base'],
  host: ['other.example:8080', 'https://other.example:8080/base'],
  hostname: ['other.example', 'https://other.example/base'],
  port: ['8080', 'https://after.example:8080/base'],
  pathname: ['/new', 'https://after.example/new'],
  search: ['?q=1', 'https://after.example/base?q=1'],
  hash: ['#frag', 'https://after.example/base#frag'],
};

for (const [name, [value, expected]] of Object.entries(setters)) {
  const url = new URL('https://before.example/old');
  url[name] = {
    toString() {
      url.href = 'https://after.example/base';
      return value;
    },
  };
  assert.strictEqual(url.href, expected, `URL.prototype.${name} setter`);
}

// A URL with an opaque path has no host to set, even when it only became
// one while the value was converted.
{
  const url = new URL('https://before.example/old');
  url.host = {
    toString() {
      url.href = 'mailto:someone@after.example';
      return 'other.example';
    },
  };
  assert.strictEqual(url.href, 'mailto:someone@after.example');
}

// The search params follow the URL the setter steps saw.
{
  const url = new URL('https://before.example/old?a=1');
  const { searchParams } = url;
  url.search = {
    toString() {
      url.href = 'https://after.example/base?b=2';
      return '?c=3';
    },
  };
  assert.strictEqual(url.href, 'https://after.example/base?c=3');
  assert.strictEqual(searchParams.toString(), 'c=3');
}

// The receiver is checked before the value is converted.
for (const name of Object.keys(setters)) {
  const { set } = Object.getOwnPropertyDescriptor(URL.prototype, name);
  let conversions = 0;
  assert.throws(() => {
    set.call({}, {
      toString() {
        conversions++;
        return '';
      },
    });
  }, {
    name: 'TypeError',
  });
  assert.strictEqual(conversions, 0, `URL.prototype.${name} setter`);
}
