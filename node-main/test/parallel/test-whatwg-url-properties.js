'use strict';
require('../common');
const assert = require('assert');
const { URL, URLSearchParams, format } = require('url');

[
  { name: 'toString' },
  { name: 'toJSON' },
  { name: Symbol.for('nodejs.util.inspect.custom') },
].forEach(({ name }) => {
  testMethod(URL.prototype, name);
});

[
  'http://www.google.com',
  'https://www.domain.com:443',
  'file:///Users/yagiz/Developer/node',
].forEach((url) => {
  const u = new URL(url);
  assert.strictEqual(JSON.stringify(u), `"${u.href}"`);
  assert.strictEqual(u.toString(), u.href);
  assert.strictEqual(format(u), u.href);
});

[
  { name: 'href' },
  { name: 'protocol' },
  { name: 'username' },
  { name: 'password' },
  { name: 'host' },
  { name: 'hostname' },
  { name: 'port' },
  { name: 'pathname' },
  { name: 'search' },
  { name: 'hash' },
  { name: 'origin', readonly: true },
  { name: 'searchParams', readonly: true },
].forEach(({ name, readonly = false }) => {
  testAccessor(URL.prototype, name, readonly);
});

[
  { name: 'createObjectURL' },
  { name: 'revokeObjectURL' },
].forEach(({ name }) => {
  testStaticAccessor(URL, name);
});

[
  { name: 'append' },
  { name: 'delete' },
  { name: 'get' },
  { name: 'getAll' },
  { name: 'has' },
  { name: 'set' },
  { name: 'sort' },
  { name: 'entries' },
  { name: 'forEach' },
  { name: 'keys' },
  { name: 'values' },
  { name: 'toString' },
  { name: Symbol.iterator, methodName: 'entries' },
  { name: Symbol.for('nodejs.util.inspect.custom') },
].forEach(({ name, methodName }) => {
  testMethod(URLSearchParams.prototype, name, methodName);
});

{
  const params = new URLSearchParams();
  params.append('a', 'b');
  params.append('a', 'c');
  params.append('b', 'c');
  assert.strictEqual(params.size, 3);
}

{
  const u = new URL('https://abc.com/?q=old');
  const s = u.searchParams;
  u.href = 'http://abc.com/?q=new';
  assert.strictEqual(s.get('q'), 'new');
}

function stringifyName(name) {
  if (typeof name === 'symbol') {
    const { description } = name;
    if (description === undefined) {
      return '';
    }
    return `[${description}]`;
  }

  return name;
}

function testMethod(target, name, methodName = stringifyName(name)) {
  const desc = Object.getOwnPropertyDescriptor(target, name);
  assert.notStrictEqual(desc, undefined);
  assert.strictEqual(desc.enumerable, typeof name === 'string');

  const { value } = desc;
  assert.strictEqual(typeof value, 'function');
  assert.strictEqual(value.name, methodName);
  assert.strictEqual(
    Object.hasOwn(value, 'prototype'),
    false,
  );
}

function testAccessor(target, name, readonly = false) {
  const desc = Object.getOwnPropertyDescriptor(target, name);
  assert.notStrictEqual(desc, undefined);
  assert.strictEqual(desc.enumerable, typeof name === 'string');

  const methodName = stringifyName(name);
  const { get, set } = desc;
  assert.strictEqual(typeof get, 'function');
  assert.strictEqual(get.name, `get ${methodName}`);
  assert.strictEqual(
    Object.hasOwn(get, 'prototype'),
    false,
  );

  if (readonly) {
    assert.strictEqual(set, undefined);
  } else {
    assert.strictEqual(typeof set, 'function');
    assert.strictEqual(set.name, `set ${methodName}`);
    assert.strictEqual(
      Object.hasOwn(set, 'prototype'),
      false,
    );
  }
}

function testStaticAccessor(target, name) {
  const desc = Object.getOwnPropertyDescriptor(target, name);
  assert.notStrictEqual(desc, undefined);

  assert.strictEqual(desc.configurable, true);
  assert.strictEqual(desc.enumerable, true);
  assert.strictEqual(desc.writable, true);
}
