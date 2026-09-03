'use strict';

// Tests for the URLSearchParams parse / serialize / toUSVString fast paths.

require('../common');
const assert = require('assert');

{
  const params = new URLSearchParams('?a=b');
  assert.strictEqual(params.toString(), 'a=b');
  assert.strictEqual(params.get('a'), 'b');
}

{
  const params = new URLSearchParams('a=b&c');
  assert.deepStrictEqual([...params], [['a', 'b'], ['c', '']]);
}

{
  const params = new URLSearchParams('&a&&& &&&&&a+b=& c&m%c3%b8%c3%b8');
  assert.ok(params.has('a'));
  assert.ok(params.has('a b'));
  assert.ok(params.has(' '));
  assert.ok(params.has(' c'));
  assert.ok(params.has('møø'));
  assert.strictEqual(params.get('a+b'), null);
}

{
  const params = new URLSearchParams('id=0&value=%');
  assert.strictEqual(params.get('id'), '0');
  assert.strictEqual(params.get('value'), '%');
}

{
  const params = new URLSearchParams('b=%2sf%2a');
  assert.strictEqual(params.get('b'), '%2sf*');
}

{
  const params = new URLSearchParams('a=b=c&d=');
  assert.strictEqual(params.get('a'), 'b=c');
  assert.strictEqual(params.get('d'), '');
}

{
  const params = new URLSearchParams('foo=bar&baz=quux');
  assert.strictEqual(params.toString(), 'foo=bar&baz=quux');
  assert.strictEqual(params.toString(), 'foo=bar&baz=quux');
  params.append('xyzzy', 'thud');
  assert.strictEqual(params.toString(), 'foo=bar&baz=quux&xyzzy=thud');
  params.set('baz', 'updated');
  assert.strictEqual(params.toString(), 'foo=bar&baz=updated&xyzzy=thud');
  params.delete('foo');
  assert.strictEqual(params.toString(), 'baz=updated&xyzzy=thud');
  params.sort();
  assert.strictEqual(params.toString(), 'baz=updated&xyzzy=thud');
}

{
  const original = new URLSearchParams('a=1&b=2');
  assert.strictEqual(original.toString(), 'a=1&b=2');
  const copy = new URLSearchParams(original);
  assert.strictEqual(copy.toString(), 'a=1&b=2');
  original.append('c', '3');
  assert.strictEqual(original.toString(), 'a=1&b=2&c=3');
  assert.strictEqual(copy.toString(), 'a=1&b=2');
}

{
  const params = new URLSearchParams({ foo: 'bar', baz: 1, xyzzy: false });
  assert.strictEqual(params.get('foo'), 'bar');
  assert.strictEqual(params.get('baz'), '1');
  assert.strictEqual(params.get('xyzzy'), 'false');
  assert.strictEqual(params.toString(), 'foo=bar&baz=1&xyzzy=false');
}

{
  const params = new URLSearchParams([['foo', 'bar'], ['baz', 'quux']]);
  assert.strictEqual(params.toString(), 'foo=bar&baz=quux');
  assert.ok(params.has('foo', 'bar'));
  assert.deepStrictEqual(params.getAll('foo'), ['bar']);
}

{
  const params = new URLSearchParams('\uD83D');
  assert.strictEqual(params.keys().next().value, '\uFFFD');
  assert.strictEqual(params.toString(), '%EF%BF%BD=');
}

{
  const params = new URLSearchParams('a=b+c&d=%20');
  assert.strictEqual(params.get('a'), 'b c');
  assert.strictEqual(params.get('d'), ' ');
  assert.strictEqual(params.toString(), 'a=b+c&d=+');
}

{
  // Fake percent-encoding must not be UTF-8-decoded into U+FFFD.
  const params = new URLSearchParams('foo=%©ar&baz=%A©uux&xyzzy=%©ud');
  assert.deepStrictEqual([...params], [
    ['foo', '%©ar'],
    ['baz', '%A©uux'],
    ['xyzzy', '%©ud'],
  ]);
  assert.strictEqual(params.toString(), 'foo=%25%C2%A9ar&baz=%25A%C2%A9uux&xyzzy=%25%C2%A9ud');
}

{
  const url = new URL('https://example.org/?foo=bar');
  const params = url.searchParams;
  assert.strictEqual(params.toString(), 'foo=bar');
  params.append('baz', 'quux');
  assert.strictEqual(url.search, '?foo=bar&baz=quux');
  assert.strictEqual(params.toString(), 'foo=bar&baz=quux');
}
