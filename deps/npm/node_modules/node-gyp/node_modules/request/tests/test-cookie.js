var Cookie = require('../vendor/cookie')
  , assert = require('assert');

var str = 'Sid="s543qactge.wKE61E01Bs%2BKhzmxrwrnug="; Path=/; httpOnly; Expires=Sat, 04 Dec 2010 23:27:28 GMT';
var cookie = new Cookie(str);

// test .toString()
assert.equal(cookie.toString(), str);

// test .path
assert.equal(cookie.path, '/');

// test .httpOnly
assert.equal(cookie.httpOnly, true);

// test .name
assert.equal(cookie.name, 'Sid');

// test .value
assert.equal(cookie.value, '"s543qactge.wKE61E01Bs%2BKhzmxrwrnug="');

// test .expires
assert.equal(cookie.expires instanceof Date, true);

// test .path default
var cookie = new Cookie('foo=bar', { url: 'http://foo.com/bar' });
assert.equal(cookie.path, '/bar');

console.log('All tests passed');
