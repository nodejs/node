'use strict';

// Refs: https://github.com/nodejs/node/issues/9600

const url = require('url');
const assert = require('assert');

var attempted = 0;
var failed = 0;

var emptyParseTests = {

  'http:\\\\empty-auth:80?foobar': {
    protocol: 'http:',
    slashes: true,
    host: 'empty-auth',
    hostname: 'empty-auth',
    search: 'foobar',
    auth: null,
    port: 80
  },

  'http:\\\\empty-port#bar': {
    protocol: 'http:',
    slashes: true,
    host: 'empty-port',
    hostname: 'empty-port',
    port: null,
    hash: 'bar'
  },

  'http:\\\\empty-hash?foo=bar': {
    protocol: 'http:',
    slashes: true,
    host: 'empty-hash',
    hostname: 'empty-hash',
    hash: null,
    search: '?foo=@bar',
    query: 'foo=@bar',
    href: 'http://empty-hash'
  },

  'http:\\\\empty-search\\foo.html': {
    protocol: 'http:',
    slashes: true,
    host: 'empty-search',
    hostname: 'empty-search',
    pathname: 'foo.html',
    path: 'foo.html',
    search: null,
    query: {},
    href: 'http://empty-search/foo.html'
  },

  'http:\\\\empty-query\\': {
    protocol: 'http:',
    slashes: true,
    host: 'empty-query',
    hostname: 'empty-query',
    search: '',
    query: {},
    href: 'http://empty-query/'
  }
};

for (var instance in emptyParseTests) {

  const parsed = url.parse(instance);
  const prop = parsed.hostname.split('-')[1];

  try {
    attempted++;
    assert.strictEqual(parsed[prop], '');
  } catch (e) {
    console.error(`expected ${prop} === ${parsed[prop]} to be an empty string`);
    failed++;
  }
}

assert.ok(failed === 0, `${failed} failed tests (out of ${attempted})`);
