'use strict';
const common = require('../common');
const assert = require('assert');
const url = require('url');

// https://github.com/joyent/node/issues/568
[
  [undefined, 'undefined'],
  [null, 'object'],
  [true, 'boolean'],
  [false, 'boolean'],
  [0.0, 'number'],
  [0, 'number'],
  [[], 'object'],
  [{}, 'object'],
  [() => {}, 'function'],
  [Symbol('foo'), 'symbol'],
].forEach(([val, type]) => {
  assert.throws(() => {
    url.parse(val);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "url" argument must be of type string.' +
             common.invalidArgTypeHelper(val)
  });
});

assert.throws(() => { url.parse('http://%E0%A4%A@fail'); },
              (e) => {
                // The error should be a URIError.
                if (!(e instanceof URIError))
                  return false;

                // The error should be from the JS engine and not from Node.js.
                // JS engine errors do not have the `code` property.
                return e.code === undefined;
              });

if (common.hasIntl) {
  // An array of Unicode code points whose Unicode NFKD contains a "bad
  // character".
  const badIDNA = (() => {
    const BAD_CHARS = '#%/:?@[\\]^|';
    const out = [];
    for (let i = 0x80; i < 0x110000; i++) {
      const cp = String.fromCodePoint(i);
      for (const badChar of BAD_CHARS) {
        if (cp.normalize('NFKD').includes(badChar)) {
          out.push(cp);
        }
      }
    }
    return out;
  })();

  // The generation logic above should at a minimum produce these two
  // characters.
  assert(badIDNA.includes('℀'));
  assert(badIDNA.includes('＠'));

  for (const badCodePoint of badIDNA) {
    const badURL = `http://fail${badCodePoint}fail.com/`;
    assert.throws(() => { url.parse(badURL); },
                  (e) => e.code === 'ERR_INVALID_URL',
                  `parsing ${badURL}`);
  }

  assert.throws(() => { url.parse('http://\u00AD/bad.com/'); },
                (e) => e.code === 'ERR_INVALID_URL',
                'parsing http://\u00AD/bad.com/');
}
