// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');

const errors = require('internal/errors');

assert.ok(errors.Error);
assert.ok(errors.TypeError);
assert.ok(errors.RangeError);

assert.throws(() => {
  new errors.Error('SOME KEY THAT DOES NOT EXIST');
}, /An invalid error message key was used/);

// Test Unknown Signal error
{
  // Unknown signal: ${sig}
  const err = new errors.Error('UNKNOWNSIGNAL', 'FOO');
  assert.strictEqual(err.name, 'Error[UNKNOWNSIGNAL]');
  assert.strictEqual(err.code, 'UNKNOWNSIGNAL');
  assert.strictEqual(err.message, 'Unknown signal: FOO');
}

// Test Invalid Argument errors
{
  // '${arg}' argument must be ${prefix} ${expected}
  const err = new errors.TypeError('INVALIDARG', 'Foo', 'Bar');
  assert.strictEqual(err.name, 'TypeError[INVALIDARG]');
  assert.strictEqual(err.code, 'INVALIDARG');
  assert.strictEqual(err.message, '"Foo" argument must be a Bar');
}

{
  // '${arg}' argument must be ${prefix} ${expected}
  const err = new errors.TypeError('INVALIDARG', 'Foo', 'Object');
  assert.strictEqual(err.name, 'TypeError[INVALIDARG]');
  assert.strictEqual(err.code, 'INVALIDARG');
  assert.strictEqual(err.message, '"Foo" argument must be an Object');
}

{
  // '${arg}' argument must be ${prefix} ${expected}
  const err = new errors.TypeError('INVALIDARG', 'Foo', ['Object']);
  assert.strictEqual(err.name, 'TypeError[INVALIDARG]');
  assert.strictEqual(err.code, 'INVALIDARG');
  assert.strictEqual(err.message, '"Foo" argument must be an Object');
}

{
  // '${arg}' argument must be one of: ${expected}
  const err = new errors.TypeError('INVALIDARG', 'Foo', ['Object', 'Bar']);
  assert.strictEqual(err.name, 'TypeError[INVALIDARG]');
  assert.strictEqual(err.code, 'INVALIDARG');
  assert.strictEqual(err.message,
                     '"Foo" argument must be one of: Object, Bar');
}

// Test Required Argument Error
{
  // `'${arg}' argument is required and cannot be undefined.`
  const err = new errors.TypeError('REQUIREDARG', 'Foo');
  assert.strictEqual(err.name, 'TypeError[REQUIREDARG]');
  assert.strictEqual(err.code, 'REQUIREDARG');
  assert.strictEqual(err.message,
                     '"Foo" argument is required and cannot be undefined');
}

// Test Invalid Option Value Error
{
  // Invalid value for "${opt}" option.
  const err = new errors.TypeError('INVALIDOPTVALUE', 'Foo');
  assert.strictEqual(err.name, 'TypeError[INVALIDOPTVALUE]');
  assert.strictEqual(err.code, 'INVALIDOPTVALUE');
  assert.strictEqual(err.message,
                     'Invalid value for "Foo" option');
}

{
  // Invalid value for "${opt}" option: ${val}.
  const err = new errors.TypeError('INVALIDOPTVALUE', 'Foo', 'Bar');
  assert.strictEqual(err.name, 'TypeError[INVALIDOPTVALUE]');
  assert.strictEqual(err.code, 'INVALIDOPTVALUE');
  assert.strictEqual(err.message,
                     'Invalid value for "Foo" option: Bar');
}

// Test Invalid Argument Value Error
{
  // Invalid value for "${arg}" argument.
  const err = new errors.TypeError('INVALIDARGVALUE', 'Foo');
  assert.strictEqual(err.name, 'TypeError[INVALIDARGVALUE]');
  assert.strictEqual(err.code, 'INVALIDARGVALUE');
  assert.strictEqual(err.message,
                     'Invalid value for "Foo" argument');
}

{
  // Invalid value for "${arg}" argument: ${val}.
  const err = new errors.TypeError('INVALIDARGVALUE', 'Foo', 'Bar');
  assert.strictEqual(err.name, 'TypeError[INVALIDARGVALUE]');
  assert.strictEqual(err.code, 'INVALIDARGVALUE');
  assert.strictEqual(err.message,
                     'Invalid value for "Foo" argument: Bar');
}
