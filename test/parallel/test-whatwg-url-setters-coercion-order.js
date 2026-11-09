'use strict';

// Tests below are not from WPT.
//
// Web IDL converts an assigned value to the attribute's type before the
// setter's steps run. A `toString()` that mutates the URL during conversion
// must be observed by the setter: the new component value applies to the
// mutated URL, not to stale state captured before the conversion.
// Refs: https://github.com/nodejs/node/issues/67345
require('../common');
const assert = require('assert');

const cases = [
  ['protocol', 'http', 'http://after.example/base'],
  ['username', 'user', 'https://user@after.example/base'],
  ['password', 'pass', 'https://:pass@after.example/base'],
  ['host', 'other.example:8080', 'https://other.example:8080/base'],
  ['hostname', 'other.example', 'https://other.example/base'],
  ['port', '8080', 'https://after.example:8080/base'],
  ['pathname', '/new', 'https://after.example/new'],
  ['search', '?q=1', 'https://after.example/base?q=1'],
  ['hash', '#frag', 'https://after.example/base#frag'],
];

for (const [setter, value, expected] of cases) {
  const url = new URL('https://before.example/old');
  url[setter] = {
    toString() {
      url.href = 'https://after.example/base';
      return value;
    },
  };
  assert.strictEqual(url.href, expected, `${setter} setter`);
}
