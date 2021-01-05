'use strict';
require('../common');
const assert = require('assert');
const { URL, URLSearchParams } = require('url');

[
  { name: 'toString' },
  { name: 'toJSON' },
  { name: Symbol.for('nodejs.util.inspect.custom') },
].forEach(({ name }) => {
  testMethod(URL.prototype, name);
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
    Object.prototype.hasOwnProperty.call(value, 'prototype'),
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
    Object.prototype.hasOwnProperty.call(get, 'prototype'),
    false,
  );

  if (readonly) {
    assert.strictEqual(set, undefined);
  } else {
    assert.strictEqual(typeof set, 'function');
    assert.strictEqual(set.name, `set ${methodName}`);
    assert.strictEqual(
      Object.prototype.hasOwnProperty.call(set, 'prototype'),
      false,
    );
  }
}
