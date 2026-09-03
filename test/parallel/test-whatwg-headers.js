'use strict';

// Tests below are not from WPT.

require('../common');
const assert = require('assert');
const util = require('util');

{
  const headers = new Headers();
  assert.strictEqual(headers.get('content-type'), null);
  assert.strictEqual(headers.has('content-type'), false);
  assert.deepStrictEqual([...headers], []);
  assert.deepStrictEqual(headers.getSetCookie(), []);
}

{
  const headers = new Headers({
    'Content-Type': 'text/plain',
    'Accept': 'application/json',
    'X-Custom': '1',
  });
  assert.strictEqual(headers.get('content-type'), 'text/plain');
  assert.strictEqual(headers.get('Content-Type'), 'text/plain');
  assert.strictEqual(headers.get('ACCEPT'), 'application/json');
  assert.ok(headers.has('accept'));
  assert.deepStrictEqual([...headers], [
    ['accept', 'application/json'],
    ['content-type', 'text/plain'],
    ['x-custom', '1'],
  ]);
}

{
  const headers = new Headers([
    ['X-A', '1'],
    ['x-b', '2'],
    ['X-A', '3'],
  ]);
  assert.strictEqual(headers.get('x-a'), '1, 3');
  assert.deepStrictEqual([...headers], [
    ['x-a', '1, 3'],
    ['x-b', '2'],
  ]);
}

{
  const source = new Headers({ 'Content-Type': 'text/html' });
  source.append('Set-Cookie', 'a=b');
  source.append('Set-Cookie', 'c=d');
  const copy = new Headers(source);
  assert.strictEqual(copy.get('content-type'), 'text/html');
  assert.deepStrictEqual(copy.getSetCookie(), ['a=b', 'c=d']);
  assert.deepStrictEqual([...copy], [...source]);
  copy.append('X-Copy', 'yes');
  assert.strictEqual(source.has('x-copy'), false);
  source.append('Set-Cookie', 'e=f');
  assert.deepStrictEqual(copy.getSetCookie(), ['a=b', 'c=d']);
}

{
  const headers = new Headers();
  headers.append('Accept', 'text/html');
  headers.append('accept', 'application/json');
  assert.strictEqual(headers.get('ACCEPT'), 'text/html, application/json');
  headers.set('ACCEPT', 'image/png');
  assert.strictEqual(headers.get('accept'), 'image/png');
  headers.delete('Accept');
  assert.strictEqual(headers.has('accept'), false);
}

{
  const headers = new Headers();
  headers.append('Cookie', 'a=1');
  headers.append('cookie', 'b=2');
  assert.strictEqual(headers.get('cookie'), 'a=1; b=2');
}

{
  const headers = new Headers();
  headers.append('set-cookie', 'a=b');
  headers.append('Set-Cookie', 'c=d');
  assert.deepStrictEqual(headers.getSetCookie(), ['a=b', 'c=d']);
  const cloned = headers.getSetCookie();
  cloned.push('e=f');
  assert.deepStrictEqual(headers.getSetCookie(), ['a=b', 'c=d']);
  headers.set('set-cookie', 'only=one');
  assert.deepStrictEqual(headers.getSetCookie(), ['only=one']);
  headers.delete('SET-COOKIE');
  assert.deepStrictEqual(headers.getSetCookie(), []);
}

{
  const headers = new Headers();
  headers.set('a', '  value  ');
  assert.strictEqual(headers.get('a'), 'value');
  headers.set('b', '\r\n\t  trimmed\t\n');
  assert.strictEqual(headers.get('b'), 'trimmed');
  headers.set('c', '\r');
  assert.strictEqual(headers.get('c'), '');
  headers.set('d', '\n');
  assert.strictEqual(headers.get('d'), '');
}

{
  const headers = new Headers();
  headers.set('a', ['b', 'c']);
  assert.strictEqual(headers.get('a'), 'b,c');
  headers.set('b', null);
  assert.strictEqual(headers.get('b'), 'null');
  headers.set('c', 1);
  assert.strictEqual(headers.get('c'), '1');
}

{
  const headers = new Headers({
    c: '5',
    b: ['3', '4'],
    a: ['1', '2'],
  });
  assert.deepStrictEqual([...headers.entries()], [
    ['a', '1,2'],
    ['b', '3,4'],
    ['c', '5'],
  ]);
}

{
  const init = [
    ['foo', '123'],
    ['bar', '456'],
  ];
  const headers = new Headers(init);
  for (const [key, val] of headers) {
    headers.delete(key);
    headers.set(`x-${key}`, val);
  }
  assert.deepStrictEqual([...headers], [
    ['foo', '123'],
    ['x-x-bar', '456'],
  ]);
}

{
  const headers = new Headers([
    ['b', '2'],
    ['c', '3'],
    ['e', '5'],
  ]);
  headers.append('d', '4');
  headers.append('a', '1');
  headers.append('f', '6');
  headers.append('c', '7');
  headers.append('abc', '8');
  assert.deepStrictEqual([...headers], [
    ['a', '1'],
    ['abc', '8'],
    ['b', '2'],
    ['c', '3, 7'],
    ['d', '4'],
    ['e', '5'],
    ['f', '6'],
  ]);
}

{
  const headers = new Headers({ 'Content-Type': 'application/json' });
  headers.set('Authorization', 'Bearer token');
  assert.strictEqual(
    util.inspect(headers, { depth: 1 }),
    "Headers { 'Content-Type': 'application/json', Authorization: 'Bearer token' }",
  );
}

{
  const headers = new Headers();
  assert.throws(() => headers.get(), TypeError);
  assert.throws(() => headers.has(), TypeError);
  assert.throws(() => headers.delete(), TypeError);
  assert.throws(() => headers.append('a'), TypeError);
  assert.throws(() => headers.set('a'), TypeError);
  assert.throws(() => headers.append('invalid @ name', 'x'), TypeError);
  assert.throws(() => headers.set('a', 'a\nb'), TypeError);
  assert.throws(() => headers.set('a', 'a\rb'), TypeError);
  assert.throws(() => headers.set('a', 'a\0b'), TypeError);
  assert.throws(() => headers.set(Symbol('x'), 'y'), TypeError);
  assert.throws(() => headers.set('a', Symbol('y')), TypeError);
  assert.throws(() => headers.set('', 'x'), TypeError);
  assert.throws(() => headers.set('a', 'héllo\u0100'), TypeError);
  assert.throws(() => new Headers(1), TypeError);
  assert.throws(() => new Headers('1'), TypeError);
  assert.throws(() => new Headers([['undici', 'fetch'], ['fetch']]), TypeError);
}

{
  assert.throws(() => Headers.prototype.get.call(null, 'a'), {
    name: 'TypeError',
    code: 'ERR_INVALID_THIS',
  });
  assert.throws(() => Headers.prototype.append.call({}, 'a', 'b'), {
    name: 'TypeError',
    code: 'ERR_INVALID_THIS',
  });
}

{
  assert.strictEqual(Headers.prototype.append.length, 2);
  assert.strictEqual(Headers.prototype.constructor.length, 0);
  assert.strictEqual(Headers.prototype.delete.length, 1);
  assert.strictEqual(Headers.prototype.get.length, 1);
  assert.strictEqual(Headers.prototype.has.length, 1);
  assert.strictEqual(Headers.prototype.set.length, 2);
  assert.strictEqual(Headers.prototype.entries, Headers.prototype[Symbol.iterator]);
  assert.strictEqual(Headers.prototype[Symbol.toStringTag], 'Headers');
  assert.strictEqual(Object.prototype.toString.call(Headers.prototype), '[object Headers]');
}

{
  const headers = new Headers();
  headers.set('content-type', 'text/plain');
  assert.strictEqual(headers.delete('content-type'), undefined);
  assert.strictEqual(headers.delete('missing'), undefined);
  assert.strictEqual(headers.set('a', 'b'), undefined);
}

{
  const headers = new Headers();
  for (const name of [
    'content-type',
    'accept',
    'user-agent',
    'cache-control',
    'set-cookie',
  ]) {
    headers.set(name, 'value');
    assert.strictEqual(headers.get(name), 'value');
    assert.ok(headers.has(name));
  }
}

{
  const headers = new Headers();
  headers.append('fhqwhgads', `a${'\t'.repeat(1000)}a`);
  assert.strictEqual(headers.get('fhqwhgads'), `a${'\t'.repeat(1000)}a`);
}
