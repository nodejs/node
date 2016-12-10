'use strict';
require('../common');
var assert = require('assert');

// test using assert
var qs = require('querystring');

function createWithNoPrototype(properties) {
  const noProto = Object.create(null);
  properties.forEach((property) => {
    noProto[property.key] = property.value;
  });
  return noProto;
}
// folding block, commented to pass gjslint
// {{{
// [ wonkyQS, canonicalQS, obj ]
var qsTestCases = [
  ['__proto__=1',
   '__proto__=1',
   createWithNoPrototype([{key: '__proto__', value: '1'}])],
  ['__defineGetter__=asdf',
   '__defineGetter__=asdf',
   JSON.parse('{"__defineGetter__":"asdf"}')],
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
      __defineGetter__: 'baz' }],
  // See: https://github.com/joyent/node/issues/3058
  ['foo&bar=baz', 'foo=&bar=baz', { foo: '', bar: 'baz' }],
  [null, '', {}],
  [undefined, '', {}]
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
  [{inf: Infinity}, 'inf=', {'inf': ''}],
  [{a: [], b: []}, '', {}]
];
// }}}

var vm = require('vm');
var foreignObject = vm.runInNewContext('({"foo": ["bar", "baz"]})');

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


function check(actual, expected) {
  assert(!(actual instanceof Object));
  assert.deepStrictEqual(Object.keys(actual).sort(),
                         Object.keys(expected).sort());
  Object.keys(expected).forEach(function(key) {
    assert.deepStrictEqual(actual[key], expected[key]);
  });
}

// test that the canonical qs is parsed properly.
qsTestCases.forEach(function(testCase) {
  check(qs.parse(testCase[0]), testCase[2]);
});

// test that the colon test cases can do the same
qsColonTestCases.forEach(function(testCase) {
  check(qs.parse(testCase[0], ';', ':'), testCase[2]);
});

// test the weird objects, that they get parsed properly
qsWeirdObjects.forEach(function(testCase) {
  check(qs.parse(testCase[1]), testCase[2]);
});

qsNoMungeTestCases.forEach(function(testCase) {
  assert.deepStrictEqual(testCase[0], qs.stringify(testCase[1], '&', '='));
});

// test the nested qs-in-qs case
{
  const f = qs.parse('a=b&q=x%3Dy%26y%3Dz');
  check(f, createWithNoPrototype([
    { key: 'a', value: 'b'},
    {key: 'q', value: 'x=y&y=z'}
  ]));

  f.q = qs.parse(f.q);
  const expectedInternal = createWithNoPrototype([
    { key: 'x', value: 'y'},
    {key: 'y', value: 'z' }
  ]);
  check(f.q, expectedInternal);
}

// nested in colon
{
  const f = qs.parse('a:b;q:x%3Ay%3By%3Az', ';', ':');
  check(f, createWithNoPrototype([
    {key: 'a', value: 'b'},
    {key: 'q', value: 'x:y;y:z'}
  ]));
  f.q = qs.parse(f.q, ';', ':');
  const expectedInternal = createWithNoPrototype([
    { key: 'x', value: 'y'},
    {key: 'y', value: 'z' }
  ]);
  check(f.q, expectedInternal);
}

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

// invalid surrogate pair throws URIError
assert.throws(function() {
  qs.stringify({ foo: '\udc00' });
}, URIError);

// coerce numbers to string
assert.strictEqual('foo=0', qs.stringify({ foo: 0 }));
assert.strictEqual('foo=0', qs.stringify({ foo: -0 }));
assert.strictEqual('foo=3', qs.stringify({ foo: 3 }));
assert.strictEqual('foo=-72.42', qs.stringify({ foo: -72.42 }));
assert.strictEqual('foo=', qs.stringify({ foo: NaN }));
assert.strictEqual('foo=', qs.stringify({ foo: Infinity }));

// nested
{
  const f = qs.stringify({
    a: 'b',
    q: qs.stringify({
      x: 'y',
      y: 'z'
    })
  });
  assert.equal(f, 'a=b&q=x%3Dy%26y%3Dz');
}

assert.doesNotThrow(function() {
  qs.parse(undefined);
});

// nested in colon
{
  const f = qs.stringify({
    a: 'b',
    q: qs.stringify({
      x: 'y',
      y: 'z'
    }, ';', ':')
  }, ';', ':');
  assert.equal(f, 'a:b;q:x%3Ay%3By%3Az');
}

check(qs.parse(), {});


// Test limiting
assert.equal(
    Object.keys(qs.parse('a=1&b=1&c=1', null, null, { maxKeys: 1 })).length,
    1);

// Test removing limit
function testUnlimitedKeys() {
  const query = {};

  for (var i = 0; i < 2000; i++) query[i] = i;

  const url = qs.stringify(query);

  assert.equal(
      Object.keys(qs.parse(url, null, null, { maxKeys: 0 })).length,
      2000);
}
testUnlimitedKeys();


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


// Test custom decode
function demoDecode(str) {
  return str + str;
}
check(qs.parse('a=a&b=b&c=c', null, null, { decodeURIComponent: demoDecode }),
      { aa: 'aa', bb: 'bb', cc: 'cc' });


// Test custom encode
function demoEncode(str) {
  return str[0];
}
var obj = { aa: 'aa', bb: 'bb', cc: 'cc' };
assert.equal(
  qs.stringify(obj, null, null, { encodeURIComponent: demoEncode }),
  'a=a&b=b&c=c');

// test overriding .unescape
var prevUnescape = qs.unescape;
qs.unescape = function(str) {
  return str.replace(/o/g, '_');
};
check(qs.parse('foo=bor'), createWithNoPrototype([{key: 'f__', value: 'b_r'}]));
qs.unescape = prevUnescape;

// test separator and "equals" parsing order
check(qs.parse('foo&bar', '&', '&'), { foo: '', bar: '' });
