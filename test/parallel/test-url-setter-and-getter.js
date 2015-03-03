var common = require('../common');
var assert = require('assert');

var url = require('url'),
    util = require('util');

var example = 'https://example.com/path?query=value';
var obj = url.parse(example);

// test pathname
obj.pathname = 'path2';
assert.equal(obj.path, 'path2?query=value');
assert.equal(obj.format(), 'https://example.com/path2?query=value');

// test pathname with ? -> %3F
obj.pathname = 'path3?';
assert.equal(obj.pathname, 'path3%3F');
assert.equal(obj.path, 'path3%3F?query=value');
assert.equal(obj.format(), 'https://example.com/path3%3F?query=value');

// test pathname with # -> %23
obj.pathname = 'path3#';
assert.equal(obj.pathname, 'path3%23');
assert.equal(obj.path, 'path3%23?query=value');
assert.equal(obj.format(), 'https://example.com/path3%23?query=value');

// resume the pathname for the following tests
obj.pathname = 'path2';

// test search
obj.search = '?foo=bar';
assert.equal(obj.search, '?foo=bar');
assert.equal(obj.path, 'path2?foo=bar');
assert.equal(obj.format(), 'https://example.com/path2?foo=bar');

// test search without ?
obj.search = 'foo=bar2';
assert.equal(obj.search, '?foo=bar2');
assert.equal(obj.path, 'path2?foo=bar2');
assert.equal(obj.format(), 'https://example.com/path2?foo=bar2');

// test query as string
obj.query = 'foo=bar3';
assert.equal(obj.query, 'foo=bar3');
assert.equal(obj.search, '?foo=bar3');
assert.equal(obj.path, 'path2?foo=bar3');
assert.equal(obj.format(), 'https://example.com/path2?foo=bar3');

// test query as object
obj.query = {foo: 'bar4'};
assert.deepEqual(obj.query, {foo: 'bar4'});
assert.equal(obj.search, '?foo=bar4');
assert.equal(obj.path, 'path2?foo=bar4');
assert.equal(obj.format(), 'https://example.com/path2?foo=bar4');
