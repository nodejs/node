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

var common = require('../common');
var assert = require('assert');

// test using assert
var qs = require('querystring');

// folding block, commented to pass gjslint
// {{{
// [ wonkyQS, canonicalQS, obj ]
var qsTestCases = [
  ['foo=918854443121279438895193',
   'foo=918854443121279438895193',
   {'foo': '918854443121279438895193'}],
  ['foo=bar', 'foo=bar', {'foo': 'bar'}],
  ['foo=bar&foo=quux', 'foo=bar&foo=quux', {'foo': ['bar', 'quux']}],
  ['foo=1&bar=2', 'foo=1&bar=2', {'foo': '1', 'bar': '2'}],
  ['my+weird+field=q1%212%22%27w%245%267%2Fz8%29%3F',
   'my%20weird%20field=q1!2%22\'w%245%267%2Fz8)%3F',
   {'my weird field': 'q1!2"\'w$5&7/z8)?' }],
  ['foo%3Dbaz=bar', 'foo%3Dbaz=bar', {'foo=baz': 'bar'}],
  ['foo=baz=bar', 'foo=baz%3Dbar', {'foo': 'baz=bar'}],
  ['str=foo&arr=1&arr=2&arr=3&somenull=&undef=',
   'str=foo&arr=1&arr=2&arr=3&somenull=&undef=',
   { 'str': 'foo',
     'arr': ['1', '2', '3'],
     'somenull': '',
     'undef': ''}],
  [' foo = bar ', '%20foo%20=%20bar%20', {' foo ': ' bar '}],
  ['foo=%zx', 'foo=%25zx', {'foo': '%zx'}],
  ['foo=%EF%BF%BD', 'foo=%EF%BF%BD', {'foo': '\ufffd' }],
  // See: https://github.com/joyent/node/issues/1707
  ['hasOwnProperty=x&toString=foo&valueOf=bar&__defineGetter__=baz',
    'hasOwnProperty=x&toString=foo&valueOf=bar&__defineGetter__=baz',
    { hasOwnProperty: 'x',
      toString: 'foo',
      valueOf: 'bar',
      __defineGetter__: 'baz' }]
];

// [ wonkyQS, canonicalQS, obj ]
var qsColonTestCases = [
  ['foo:bar', 'foo:bar', {'foo': 'bar'}],
  ['foo:bar;foo:quux', 'foo:bar;foo:quux', {'foo': ['bar', 'quux']}],
  ['foo:1&bar:2;baz:quux',
   'foo:1%26bar%3A2;baz:quux',
   {'foo': '1&bar:2', 'baz': 'quux'}],
  ['foo%3Abaz:bar', 'foo%3Abaz:bar', {'foo:baz': 'bar'}],
  ['foo:baz:bar', 'foo:baz%3Abar', {'foo': 'baz:bar'}]
];

// [wonkyObj, qs, canonicalObj]
var extendedFunction = function() {};
extendedFunction.prototype = {a: 'b'};
var qsWeirdObjects = [
  [{regexp: /./g}, 'regexp=', {'regexp': ''}],
  [{regexp: new RegExp('.', 'g')}, 'regexp=', {'regexp': ''}],
  [{fn: function() {}}, 'fn=', {'fn': ''}],
  [{fn: new Function('')}, 'fn=', {'fn': ''}],
  [{math: Math}, 'math=', {'math': ''}],
  [{e: extendedFunction}, 'e=', {'e': ''}],
  [{d: new Date()}, 'd=', {'d': ''}],
  [{d: Date}, 'd=', {'d': ''}],
  [{f: new Boolean(false), t: new Boolean(true)}, 'f=&t=', {'f': '', 't': ''}],
  [{f: false, t: true}, 'f=false&t=true', {'f': 'false', 't': 'true'}],
  [{n: null}, 'n=', {'n': ''}],
  [{nan: NaN}, 'nan=', {'nan': ''}],
  [{inf: Infinity}, 'inf=', {'inf': ''}]
];
// }}}

var Script = require('vm').Script;
var foreignObject = Script.runInContext('({"foo": ["bar", "baz"]})',
                                        Script.createContext());

var qsNoMungeTestCases = [
  ['', {}],
  ['foo=bar&foo=baz', {'foo': ['bar', 'baz']}],
  ['foo=bar&foo=baz', foreignObject],
  ['blah=burp', {'blah': 'burp'}],
  ['gragh=1&gragh=3&goo=2', {'gragh': ['1', '3'], 'goo': '2'}],
  ['frappucino=muffin&goat%5B%5D=scone&pond=moose',
   {'frappucino': 'muffin', 'goat[]': 'scone', 'pond': 'moose'}],
  ['trololol=yes&lololo=no', {'trololol': 'yes', 'lololo': 'no'}]
];

assert.strictEqual('918854443121279438895193',
                   qs.parse('id=918854443121279438895193').id);

// test that the canonical qs is parsed properly.
qsTestCases.forEach(function(testCase) {
  assert.deepEqual(testCase[2], qs.parse(testCase[0]));
});

// test that the colon test cases can do the same
qsColonTestCases.forEach(function(testCase) {
  assert.deepEqual(testCase[2], qs.parse(testCase[0], ';', ':'));
});

// test the weird objects, that they get parsed properly
qsWeirdObjects.forEach(function(testCase) {
  assert.deepEqual(testCase[2], qs.parse(testCase[1]));
});

qsNoMungeTestCases.forEach(function(testCase) {
  assert.deepEqual(testCase[0], qs.stringify(testCase[1], '&', '=', false));
});

// test the nested qs-in-qs case
(function() {
  var f = qs.parse('a=b&q=x%3Dy%26y%3Dz');
  f.q = qs.parse(f.q);
  assert.deepEqual(f, { a: 'b', q: { x: 'y', y: 'z' } });
})();

// nested in colon
(function() {
  var f = qs.parse('a:b;q:x%3Ay%3By%3Az', ';', ':');
  f.q = qs.parse(f.q, ';', ':');
  assert.deepEqual(f, { a: 'b', q: { x: 'y', y: 'z' } });
})();

// now test stringifying

// basic
qsTestCases.forEach(function(testCase) {
  assert.equal(testCase[1], qs.stringify(testCase[2]));
});

qsColonTestCases.forEach(function(testCase) {
  assert.equal(testCase[1], qs.stringify(testCase[2], ';', ':'));
});

qsWeirdObjects.forEach(function(testCase) {
  assert.equal(testCase[1], qs.stringify(testCase[0]));
});

// nested
var f = qs.stringify({
  a: 'b',
  q: qs.stringify({
    x: 'y',
    y: 'z'
  })
});
assert.equal(f, 'a=b&q=x%3Dy%26y%3Dz');

assert.doesNotThrow(function() {
  qs.parse(undefined);
});

// nested in colon
var f = qs.stringify({
  a: 'b',
  q: qs.stringify({
    x: 'y',
    y: 'z'
  }, ';', ':')
}, ';', ':');
assert.equal(f, 'a:b;q:x%3Ay%3By%3Az');


assert.deepEqual({}, qs.parse());



var b = qs.unescapeBuffer('%d3%f2Ug%1f6v%24%5e%98%cb' +
                          '%0d%ac%a2%2f%9d%eb%d8%a2%e6');
// <Buffer d3 f2 55 67 1f 36 76 24 5e 98 cb 0d ac a2 2f 9d eb d8 a2 e6>
assert.equal(0xd3, b[0]);
assert.equal(0xf2, b[1]);
assert.equal(0x55, b[2]);
assert.equal(0x67, b[3]);
assert.equal(0x1f, b[4]);
assert.equal(0x36, b[5]);
assert.equal(0x76, b[6]);
assert.equal(0x24, b[7]);
assert.equal(0x5e, b[8]);
assert.equal(0x98, b[9]);
assert.equal(0xcb, b[10]);
assert.equal(0x0d, b[11]);
assert.equal(0xac, b[12]);
assert.equal(0xa2, b[13]);
assert.equal(0x2f, b[14]);
assert.equal(0x9d, b[15]);
assert.equal(0xeb, b[16]);
assert.equal(0xd8, b[17]);
assert.equal(0xa2, b[18]);
assert.equal(0xe6, b[19]);

