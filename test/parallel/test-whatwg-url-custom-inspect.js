'use strict';

// Tests below are not from WPT.

const common = require('../common');
if (!common.hasIntl) {
  // A handful of the tests fail when ICU is not included.
  common.skip('missing Intl');
}

const util = require('util');
const URL = require('url').URL;
const assert = require('assert');

const url = new URL('https://username:password@host.name:8080/path/name/?que=ry#hash');

assert.strictEqual(
  util.inspect(url),
  `URL {
  href: 'https://username:password@host.name:8080/path/name/?que=ry#hash',
  origin: 'https://host.name:8080',
  protocol: 'https:',
  username: 'username',
  password: 'password',
  host: 'host.name:8080',
  hostname: 'host.name',
  port: '8080',
  pathname: '/path/name/',
  search: '?que=ry',
  searchParams: URLSearchParams { 'que' => 'ry' },
  hash: '#hash'
}`);

assert.strictEqual(
  util.inspect(url, { showHidden: true }),
  `URL {
  href: 'https://username:password@host.name:8080/path/name/?que=ry#hash',
  origin: 'https://host.name:8080',
  protocol: 'https:',
  username: 'username',
  password: 'password',
  host: 'host.name:8080',
  hostname: 'host.name',
  port: '8080',
  pathname: '/path/name/',
  search: '?que=ry',
  searchParams: URLSearchParams { 'que' => 'ry' },
  hash: '#hash',
  cannotBeBase: false,
  special: true,
  [Symbol(context)]: URLContext {
    flags: 2032,
    scheme: 'https:',
    username: 'username',
    password: 'password',
    host: 'host.name',
    port: 8080,
    path: [ 'path', 'name', '', [length]: 3 ],
    query: 'que=ry',
    fragment: 'hash'
  }
}`);

assert.strictEqual(
  util.inspect({ a: url }, { depth: 0 }),
  '{ a: [URL] }');

class MyURL extends URL {}
assert(util.inspect(new MyURL(url.href)).startsWith('MyURL {'));
