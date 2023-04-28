'use strict';
require('../common');
const assert = require('assert');
const url = require('url');

function createWithNoPrototype(properties = []) {
  const noProto = { __proto__: null };
  properties.forEach((property) => {
    noProto[property.key] = property.value;
  });
  return noProto;
}

function check(actual, expected) {
  assert.notStrictEqual(Object.getPrototypeOf(actual), Object.prototype);
  assert.deepStrictEqual(Object.keys(actual).sort(),
                         Object.keys(expected).sort());
  Object.keys(expected).forEach(function(key) {
    assert.deepStrictEqual(actual[key], expected[key]);
  });
}

const parseTestsWithQueryString = {
  '/foo/bar?baz=quux#frag': {
    href: '/foo/bar?baz=quux#frag',
    hash: '#frag',
    search: '?baz=quux',
    query: createWithNoPrototype([{ key: 'baz', value: 'quux' }]),
    pathname: '/foo/bar',
    path: '/foo/bar?baz=quux'
  },
  'http://example.com': {
    href: 'http://example.com/',
    protocol: 'http:',
    slashes: true,
    host: 'example.com',
    hostname: 'example.com',
    query: createWithNoPrototype(),
    search: null,
    pathname: '/',
    path: '/'
  },
  '/example': {
    protocol: null,
    slashes: null,
    auth: undefined,
    host: null,
    port: null,
    hostname: null,
    hash: null,
    search: null,
    query: createWithNoPrototype(),
    pathname: '/example',
    path: '/example',
    href: '/example'
  },
  '/example?query=value': {
    protocol: null,
    slashes: null,
    auth: undefined,
    host: null,
    port: null,
    hostname: null,
    hash: null,
    search: '?query=value',
    query: createWithNoPrototype([{ key: 'query', value: 'value' }]),
    pathname: '/example',
    path: '/example?query=value',
    href: '/example?query=value'
  }
};
for (const u in parseTestsWithQueryString) {
  const actual = url.parse(u, true);
  const expected = Object.assign(new url.Url(), parseTestsWithQueryString[u]);
  for (const i in actual) {
    if (actual[i] === null && expected[i] === undefined) {
      expected[i] = null;
    }
  }

  const properties = Object.keys(actual).sort();
  assert.deepStrictEqual(properties, Object.keys(expected).sort());
  properties.forEach((property) => {
    if (property === 'query') {
      check(actual[property], expected[property]);
    } else {
      assert.deepStrictEqual(actual[property], expected[property]);
    }
  });
}
