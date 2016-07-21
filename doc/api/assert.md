# Assert

    Stability: 3 - Locked

The `assert` module provides a simple set of assertion tests that can be used to
test invariants. The module is intended for internal use by Node.js, but can be
used in application code via `require('assert')`. However, `assert` is not a
testing framework, and is not intended to be used as a general purpose assertion
library.

The API for the `assert` module is [Locked][]. This means that there will be no
additions or changes to any of the methods implemented and exposed by
the module.

## assert(value[, message])
<!-- YAML
added: v0.5.9
-->

An alias of [`assert.ok()`].

## assert.deepEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->

Tests for deep equality between the `actual` and `expected` parameters.
Primitive values are compared with the equal comparison operator ( `==` ).

Only enumerable "own" properties are considered. The `deepEqual()`
implementation does not test object prototypes, attached symbols, or
non-enumerable properties. This can lead to some potentially surprising
results. For example, the following example does not throw an `AssertionError`
because the properties on the [`Error`][] object are non-enumerable:

Example: The following does not throw an `AssertionError` because the properties
on the [`Error`] object are non-enumerable

```js
const assert = require('assert');

// WARNING: This does not throw an AssertionError!
assert.deepEqual(Error('a'), Error('b'));
```

"Deep" equality means that the enumerable "own" properties of child objects
are evaluated also:

Examples:

```js
const assert = require('assert');

const obj1 = {
  a: {
    b: 1
  }
};
const obj2 = {
  a: {
    b: 2
  }
};
const obj3 = {
  a: {
    b: 1
  }
};
const obj4 = Object.create(obj1);

// OK
assert.deepEqual(obj1, obj1);

// OK
assert.deepEqual(obj1, obj3);

// Throws an exception: AssertionError: { a: { b: 1 } } deepEqual { a: { b: 2 } }
// The reason for this is that the values of `b` are different
assert.deepEqual(obj1, obj2);

// Throws an exception: AssertionError: { a: { b: 1 } } deepEqual {}
// The reason for this is that prototypes are ignored
assert.deepEqual(obj1, obj4);
```

If the values are not equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.deepStrictEqual(actual, expected[, message])
<!-- YAML
added: v1.2.0
-->

Generally identical to `assert.deepEqual()` with two exceptions. First,
primitive values are compared using the strict equality operator ( `===` ).
Second, object comparisons include a strict equality check of their prototypes.

Example:

```js
const assert = require('assert');

const obj1 = {
  a: 1
};
const obj2 = {
  a: '1'
};

// Throws an exception: AssertionError: { a: 1 } deepStrictEqual { a: '1' }
assert.deepStrictEqual(obj1, obj2);
```

If the values are not equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.doesNotThrow(block[, error][, message])
<!-- YAML
added: v0.1.21
-->

Asserts that the function `block` does not throw an error. See
[`assert.throws()`][] for more details.

When `assert.doesNotThrow()` is called, it will immediately call the `block`
function.

If an error is thrown and it is the same type as that specified by the `error`
parameter, then an `AssertionError` is thrown. If the error is of a different
type, or if the `error` parameter is undefined, the error is propagated back
to the caller.

Examples:

```js
const assert = require('assert');

// Throws an exception: TypeError: Wrong value
assert.doesNotThrow(
  () => {
    throw new TypeError('Wrong value');
  },
  SyntaxError
);

// Throws an exception: AssertionError: Got unwanted exception (TypeError)..
assert.doesNotThrow(
  () => {
    throw new TypeError('Wrong value');
  },
  TypeError
);

// Throws an exception: AssertionError: Got unwanted exception (TypeError). Whoops
assert.doesNotThrow(
  () => {
    throw new TypeError('Wrong value');
  },
  TypeError,
  'Whoops'
);
```

## assert.equal(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->

Tests shallow, coercive equality between the `actual` and `expected` parameters
using the equal comparison operator ( `==` ).

Examples:

```js
const assert = require('assert');

// OK
assert.equal(1, 1);

// OK
assert.equal(1, '1');

// Throws an exception: AssertionError: 1 == 2
assert.equal(1, 2);

// Throws an exception: AssertionError: { a: { b: 1 } } == { a: { b: 1 } }
assert.equal({a: {b: 1}}, {a: {b: 1}});
```

If the values are not equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.fail(actual, expected, message, operator)
<!-- YAML
added: v0.1.21
-->

Throws an `AssertionError`. If `message` is falsy, the error message is set as
the values of `actual` and `expected` separated by the provided `operator`.
Otherwise, the error message is the value of `message`.

Examples:

```js
const assert = require('assert');

// Throws an exception: AssertionError: 1 > 2
assert.fail(1, 2, undefined, '>');

// Throws an exception: AssertionError: whoops
assert.fail(1, 2, 'whoops', '>');
```

## assert.ifError(value)
<!-- YAML
added: v0.1.97
-->

Throws `value` if `value` is truthy. This is useful when testing the `error`
argument in callbacks.

Examples:

```js
const assert = require('assert');

// OK
assert.ifError(0);

// Throws an exception: 1
assert.ifError(1); // Throws 1

// Throws an exception: error
assert.ifError('error');

// Throws an exception: Error
assert.ifError(new Error());
```

## assert.notDeepEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->

Tests for any deep inequality. Opposite of [`assert.deepEqual()`][].

Examples:

```js
const assert = require('assert');

const obj1 = {
  a: {
    b: 1
  }
};
const obj2 = {
  a: {
    b: 2
  }
};
const obj3 = {
  a: {
    b: 1
  }
};
const obj4 = Object.create(obj1);

// OK
assert.notDeepEqual(obj1, obj2);

// OK
assert.notDeepEqual(obj1, obj4);

// Throws an exception: AssertionError: { a: { b: 1 } } notDeepEqual { a: { b: 1 } }
assert.notDeepEqual(obj1, obj1);

// Throws an exception: AssertionError: { a: { b: 1 } } notDeepEqual { a: { b: 1 } }
assert.notDeepEqual(obj1, obj3);
```

If the values are deeply equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.notDeepStrictEqual(actual, expected[, message])
<!-- YAML
added: v1.2.0
-->

Tests for deep strict inequality. Opposite of [`assert.deepStrictEqual()`][].

Examples:

```js
const assert = require('assert');

const obj1 = {
  a: 1
};
const obj2 = {
  a: '1'
};

// OK
assert.notDeepStrictEqual(obj1, obj2);
```

If the values are deeply and strictly equal, an `AssertionError` is thrown
with a `message` property set equal to the value of the `message` parameter. If
the `message` parameter is undefined, a default error message is assigned.

## assert.notEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->

Tests shallow, coercive inequality with the not equal comparison operator
( `!=` ).

Examples:

```js
const assert = require('assert');

// OK
assert.notEqual(1, 2);

// Throws an exception: AssertionError: 1 != 1
assert.notEqual(1, 1);

// Throws an exception: AssertionError: 1 != '1'
assert.notEqual(1, '1');
```

If the values are equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.notStrictEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->

Tests strict inequality as determined by the strict not equal operator
( `!==` ).

Examples:

```js
const assert = require('assert');

// OK
assert.notStrictEqual(1, 2);

// OK
assert.notStrictEqual(1, '1');

// Throws an exception: AssertionError: 1 != 1
assert.notStrictEqual(1, 1);
```

## assert.ok(value[, message])
<!-- YAML
added: v0.1.21
-->

Tests if `value` is truthy. It is equivalent to
`assert.equal(!!value, true, message)`.

If `value` is not truthy, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is `undefined`, a default error message is assigned.

Examples:

```js
const assert = require('assert');

// OK
assert.ok(true);

// OK
assert.ok(1);

// Throws an exception: AssertionError: false == true
assert.ok(false);

// Throws an exception: AssertionError: 0 == true
assert.ok(0);

// Throws an exception: AssertionError: it's false
assert.ok(false, 'it is false');
```

## assert.strictEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->

Tests strict equality as determined by the strict equality operator ( `===` ).

Examples:

```js
const assert = require('assert');

// OK
assert.strictEqual(1, 1);

// Throws an exception: AssertionError: 1 === 2
assert.strictEqual(1, 2);

// Throws an exception: AssertionError: 1 === '1'
assert.strictEqual(1, '1');
```

If the values are not strictly equal, an `AssertionError` is thrown with a
`message` property set equal to the value of the `message` parameter. If the
`message` parameter is undefined, a default error message is assigned.

## assert.throws(block[, error][, message])
<!-- YAML
added: v0.1.21
-->

Expects the function `block` to throw an error.

If specified, `error` can be a constructor, [`RegExp`][], or validation
function.

If specified, `message` will be the message provided by the `AssertionError` if
the block fails to throw.
Example: Match a thrown exception by constructor

```js
assert.throws(
  () => {
    throw new Error('Wrong value');
  },
  Error
);
```

Example: Match a thrown exception by testing its message with a [`RegExp`]

```js
assert.throws(
  () => {
    throw new Error('Wrong value');
  },
  /value/
);
```

Example: Match a thrown exception using a custom validation function

```js
assert.throws(
  () => {
    throw new Error('Wrong value');
  },
  (err) => {
    if ((err instanceof Error) && /value/.test(err)) {
      return true;
    }
  },
  'unexpected error'
);
```

Note that `error` can not be a string. If a string is provided as the second
argument, then `error` is assumed to be omitted and the string will be used for
`message` instead. This can lead to easy-to-miss mistakes.

Example:

```js
// THIS IS A MISTAKE! DO NOT DO THIS!
assert.throws(myFunction, 'missing foo', 'did not throw with expected message');

// Do this instead
assert.throws(myFunction, /missing foo/, 'did not throw with expected message');
```

[`assert.deepEqual()`]: #assert_assert_deepequal_actual_expected_message
[`assert.deepStrictEqual()`]: #assert_assert_deepstrictequal_actual_expected_message
[`assert.ok()`]: #assert_assert_ok_value_message
[`assert.throws()`]: #assert_assert_throws_block_error_message
[`Error`]: errors.html#errors_class_error
[`TypeError`]: errors.html#errors_class_typeerror

[Locked]: documentation.html#documentation_stability_index
[`RegExp`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Regular_Expressions
