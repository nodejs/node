# Assert

    Stability: 3 - Locked

The `assert` module provides a simple set of assertion tests that can be used to
test invariants. The module is intended for internal use by Node.js, but can be
used in application code via `require('assert')`. However, `assert` is not a
testing framework, and is not intended to be used as a general purpose assertion
library.

The API for the `assert` module is [Locked]. This means that there will be no
additions or changes to any of the methods implemented and exposed by this module.

## assert(value[, message])
<!-- YAML
added: v0.5.9
-->

An alias of [`assert.ok()`].

## assert.deepEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->

* `actual` {mixed} A value to check
* `expected` {mixed} The expected value of `actual`
* `message` {String} The exception message to use when `actual` and `expected`
  are not equal. If not supplied, then a generated message containing `actual`
  and `expected` is used.
* Return: `undefined`

Tests *deep* equality between `actual` and `expected` as determined by the `==`
operator. If `actual` and `expected` are not equal, then an `AssertionError`
exception is thrown.

Only enumerable "own" properties are considered. The `assert.deepEqual()`
implementation does not test object prototypes, attached symbols, or
non-enumerable properties. This can lead to some potentially surprising
results.

Example: The following does not throw an `AssertionError` because the properties
on the [`Error`] object are non-enumerable

```js
const assert = require('assert');

// WARNING: This does not throw an AssertionError!
assert.deepEqual(Error('a'), Error('b'));
```

"Deep" equality means that the enumerable "own" properties of child objects
are evaluated as well.

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

## assert.deepStrictEqual(actual, expected[, message])
<!-- YAML
added: v1.2.0
-->

* `actual` {mixed} A value to check
* `expected` {mixed} The expected value of `actual`
* `message` {String} The exception message to use when `actual` and `expected`
  are not equal. If not supplied, then a generated message containing `actual`
  and `expected` is used.

Tests *deep* equality between `actual` and `expected` as determined by the `===`
operator. If `actual` and `expected` are not equal, then an `AssertionError`
exception is thrown.

Object comparisons include a strict equal check of their prototypes.

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

## assert.doesNotThrow(block[, error][, message])
<!-- YAML
added: v0.1.21
-->

* `block` {Function} A function that is not expected to throw an exception
* `error` {Function | RegExp} A constructor, regular expression, or validation
  function to be used when matching an exception thrown by `block`
* `message` {String} A message that is appended to the exception thrown when
  `block` fails to throw an exception.
* Return: `undefined`

Calls the function `block`, not expecting an exception to be thrown. If `block`
throws an exception that matches `error`, an `AssertionError` exception is thrown.
If the exception does *not* match `error` (or `error` is not supplied), then the
original exception is re-thrown.

This function is the opposite of [`assert.throws()`].

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

* `actual` {mixed} A value to check
* `expected` {mixed} The expected value of `actual`
* `message` {String} The exception message to use when `actual` and `expected`
  are not equal. If not supplied, then a generated message containing `actual`
  and `expected` is used.
* Return: `undefined`

Tests equality between `actual` and `expected` as determined by the (shallow
and coercive) `==` operator. If `actual` and `expected` are not equal, then an
`AssertionError` exception is thrown.

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

## assert.fail(actual, expected, message, operator)
<!-- YAML
added: v0.1.21
-->

* `actual` {mixed} A value to check
* `expected` {mixed} The expected value of `actual`
* `message` {String} The exception message to use. If not supplied, then a
  generated message containing `actual` and `expected` is used.
* `operator` {String} If `message` is not specified, this is a comparison operator
  that is inserted between `actual` and `expected` in the generated exception
  message.
* Return: `undefined`

Always throws an `AssertionError`.

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

* `value` {mixed}
* Return: `undefined`

Throws `value` if `value` evaluates to `true`. This is useful when testing the
error argument in callbacks.

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

* `actual` {mixed} A value to check
* `expected` {mixed} The expected value of `actual`
* `message` {String} The exception message to use when `actual` and `expected`
  are equal. If not supplied, then a generated message containing `actual` and
  `expected` is used.

Tests *deep* inequality between `actual` and `expected` as determined by the `!=`
operator. If `actual` and `expected` are equal, then an `AssertionError`
exception is thrown.

Only enumerable "own" properties are considered. The `assert.notDeepEqual()`
implementation does not test object prototypes, attached symbols, or
non-enumerable properties. This can lead to some potentially surprising
results.

This function is the opposite of [`assert.deepEqual()`].

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

## assert.notDeepStrictEqual(actual, expected[, message])
<!-- YAML
added: v1.2.0
-->

* `actual` {mixed} A value to check
* `expected` {mixed} The expected value of `actual`
* `message` {String} The exception message to use when `actual` and `expected`
  are equal. If not supplied, then a generated message containing `actual` and
  `expected` is used.

Tests *deep* inequality between `actual` and `expected` as determined by the `!==`
operator. If `actual` and `expected` are equal, then an `AssertionError`
exception is thrown.

Object comparisons include a not strict equal check of their prototypes.

This function is the opposite of [`assert.deepStrictEqual()`].

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

## assert.notEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->

* `actual` {mixed} A value to check
* `expected` {mixed} The expected value of `actual`
* `message` {String} The exception message to use when `actual` and `expected`
  are equal. If not supplied, then a generated message containing `actual` and
  `expected` is used.

Tests inequality between `actual` and `expected` as determined by the (shallow
and coercive) `!=` operator. If `actual` and `expected` are equal, then an
`AssertionError` exception is thrown.

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

## assert.notStrictEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->

* `actual` {mixed} A value to check
* `expected` {mixed} The expected value of `actual`
* `message` {String} The exception message to use when `actual` and `expected`
  are equal. If not supplied, then a generated message containing `actual` and
  `expected` is used.

Tests inequality between `actual` and `expected` as determined by the `!==`
operator. If `actual` and `expected` are equal, then an `AssertionError` exception
is thrown.

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

* `value` {mixed} A value to check
* `message` {String} The exception message to use when `value` does not evaluate
  to `true`. If not supplied, then a generated message containing `value` is used.
* Return: `undefined`

Tests if `value` evaluates to `true`. If `value` does not evaluate to `true`,
then an `AssertionError` exception is thrown.

This function is equivalent to `assert.equal(!!value, true, message)`.

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

* `actual` {mixed} A value to check
* `expected` {mixed} The expected value of `actual`
* `message` {String} The exception message to use when `actual` and `expected`
  are not equal. If not supplied, then a generated message containing `actual`
  and `expected` is used.

Tests equality between `actual` and `expected` as determined by the `===`
operator. If `actual` and `expected` are not equal, then an `AssertionError`
exception is thrown.

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

## assert.throws(block[, error][, message])
<!-- YAML
added: v0.1.21
-->

* `block` {Function} A function that is expected to throw an exception
* `error` {Function | RegExp} A constructor, regular expression, or validation
  function to be used when matching an exception thrown by `block`
* `message` {String} The exception message to use when `block` fails to throw an
  exception. If not supplied, then a generic, generated message is used.
* Return: `undefined`

Calls the function `block`, expecting an exception to be thrown. If `block`
does not throw an exception, an `AssertionError` exception is thrown.

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
