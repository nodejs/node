# Assert

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

The `assert` module provides a simple set of assertion tests that can be used to
test invariants.

## assert(value[, message])
<!-- YAML
added: v0.5.9
-->
* `value` {any}
* `message` {any}

An alias of [`assert.ok()`][].

## assert.deepEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12142
    description: Set and Map content is also compared
  - version: v6.4.0, v4.7.1
    pr-url: https://github.com/nodejs/node/pull/8002
    description: Typed array slices are handled correctly now.
  - version: v6.1.0, v4.5.0
    pr-url: https://github.com/nodejs/node/pull/6432
    description: Objects with circular references can be used as inputs now.
  - version: v5.10.1, v4.4.3
    pr-url: https://github.com/nodejs/node/pull/5910
    description: Handle non-`Uint8Array` typed arrays correctly.
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Tests for deep equality between the `actual` and `expected` parameters.
Primitive values are compared with the [Abstract Equality Comparison][]
( `==` ).

Only [enumerable "own" properties][] are considered. The
[`assert.deepEqual()`][] implementation does not test the
[`[[Prototype]]`][prototype-spec] of objects, attached symbols, or
non-enumerable properties — for such checks, consider using
[`assert.deepStrictEqual()`][] instead. This can lead to some
potentially surprising results. For example, the following example does not
throw an `AssertionError` because the properties on the [`Error`][] object are
not enumerable:

```js
// WARNING: This does not throw an AssertionError!
assert.deepEqual(Error('a'), Error('b'));
```

An exception is made for [`Map`][] and [`Set`][]. Maps and Sets have their
contained items compared too, as expected.

"Deep" equality means that the enumerable "own" properties of child objects
are evaluated also:

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

assert.deepEqual(obj1, obj1);
// OK, object is equal to itself

assert.deepEqual(obj1, obj2);
// AssertionError: { a: { b: 1 } } deepEqual { a: { b: 2 } }
// values of b are different

assert.deepEqual(obj1, obj3);
// OK, objects are equal

assert.deepEqual(obj1, obj4);
// AssertionError: { a: { b: 1 } } deepEqual {}
// Prototypes are ignored
```

If the values are not equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.deepStrictEqual(actual, expected[, message])
<!-- YAML
added: v1.2.0
changes:
  - version: v8.0.0
    pr-url: https://github.com/nodejs/node/pull/12142
    description: Set and Map content is also compared
  - version: v6.4.0, v4.7.1
    pr-url: https://github.com/nodejs/node/pull/8002
    description: Typed array slices are handled correctly now.
  - version: v6.1.0
    pr-url: https://github.com/nodejs/node/pull/6432
    description: Objects with circular references can be used as inputs now.
  - version: v5.10.1, v4.4.3
    pr-url: https://github.com/nodejs/node/pull/5910
    description: Handle non-`Uint8Array` typed arrays correctly.
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Generally identical to `assert.deepEqual()` with three exceptions:

1. Primitive values are compared using the [Strict Equality Comparison][]
  ( `===` ). Set values and Map keys are compared using the [SameValueZero][]
  comparison. (Which means they are free of the [caveats][]).
2. [`[[Prototype]]`][prototype-spec] of objects are compared using
  the [Strict Equality Comparison][] too.
3. [Type tags][Object.prototype.toString()] of objects should be the same.

```js
const assert = require('assert');

assert.deepEqual({ a: 1 }, { a: '1' });
// OK, because 1 == '1'

assert.deepStrictEqual({ a: 1 }, { a: '1' });
// AssertionError: { a: 1 } deepStrictEqual { a: '1' }
// because 1 !== '1' using strict equality

// The following objects don't have own properties
const date = new Date();
const object = {};
const fakeDate = {};

Object.setPrototypeOf(fakeDate, Date.prototype);

assert.deepEqual(object, fakeDate);
// OK, doesn't check [[Prototype]]
assert.deepStrictEqual(object, fakeDate);
// AssertionError: {} deepStrictEqual Date {}
// Different [[Prototype]]

assert.deepEqual(date, fakeDate);
// OK, doesn't check type tags
assert.deepStrictEqual(date, fakeDate);
// AssertionError: 2017-03-11T14:25:31.849Z deepStrictEqual Date {}
// Different type tags
```

If the values are not equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.doesNotThrow(block[, error][, message])
<!-- YAML
added: v0.1.21
changes:
  - version: v5.11.0, v4.4.5
    pr-url: https://github.com/nodejs/node/pull/2407
    description: The `message` parameter is respected now.
  - version: v4.2.0
    pr-url: https://github.com/nodejs/node/pull/3276
    description: The `error` parameter can now be an arrow function.
-->
* `block` {Function}
* `error` {RegExp|Function}
* `message` {any}

Asserts that the function `block` does not throw an error. See
[`assert.throws()`][] for more details.

When `assert.doesNotThrow()` is called, it will immediately call the `block`
function.

If an error is thrown and it is the same type as that specified by the `error`
parameter, then an `AssertionError` is thrown. If the error is of a different
type, or if the `error` parameter is undefined, the error is propagated back
to the caller.

The following, for instance, will throw the [`TypeError`][] because there is no
matching error type in the assertion:

```js
assert.doesNotThrow(
  () => {
    throw new TypeError('Wrong value');
  },
  SyntaxError
);
```

However, the following will result in an `AssertionError` with the message
'Got unwanted exception (TypeError)..':

```js
assert.doesNotThrow(
  () => {
    throw new TypeError('Wrong value');
  },
  TypeError
);
```

If an `AssertionError` is thrown and a value is provided for the `message`
parameter, the value of `message` will be appended to the `AssertionError`
message:

```js
assert.doesNotThrow(
  () => {
    throw new TypeError('Wrong value');
  },
  TypeError,
  'Whoops'
);
// Throws: AssertionError: Got unwanted exception (TypeError). Whoops
```

## assert.equal(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Tests shallow, coercive equality between the `actual` and `expected` parameters
using the [Abstract Equality Comparison][] ( `==` ).

```js
const assert = require('assert');

assert.equal(1, 1);
// OK, 1 == 1
assert.equal(1, '1');
// OK, 1 == '1'

assert.equal(1, 2);
// AssertionError: 1 == 2
assert.equal({ a: { b: 1 } }, { a: { b: 1 } });
//AssertionError: { a: { b: 1 } } == { a: { b: 1 } }
```

If the values are not equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.fail([message])
## assert.fail(actual, expected[, message[, operator[, stackStartFunction]]])
<!-- YAML
added: v0.1.21
-->
* `actual` {any}
* `expected` {any}
* `message` {any} (default: 'Failed')
* `operator` {string} (default: '!=')
* `stackStartFunction` {function} (default: `assert.fail`)

Throws an `AssertionError`. If `message` is falsy, the error message is set as
the values of `actual` and `expected` separated by the provided `operator`.
If just the two `actual` and `expected` arguments are provided, `operator` will
default to `'!='`. If `message` is provided only it will be used as the error
message, the other arguments will be stored as properties on the thrown object.
If `stackStartFunction` is provided, all stack frames above that function will
be removed from stacktrace (see [`Error.captureStackTrace`]). If no arguments
are given, the default message `Failed` will be used.

```js
const assert = require('assert');

assert.fail(1, 2, undefined, '>');
// AssertionError [ERR_ASSERTION]: 1 > 2

assert.fail(1, 2, 'fail');
// AssertionError [ERR_ASSERTION]: fail

assert.fail(1, 2, 'whoops', '>');
// AssertionError [ERR_ASSERTION]: whoops
```

*Note*: Is the last two cases `actual`, `expected`, and `operator` have no
influence on the error message.

```js
assert.fail();
// AssertionError [ERR_ASSERTION]: Failed

assert.fail('boom');
// AssertionError [ERR_ASSERTION]: boom

assert.fail('a', 'b');
// AssertionError [ERR_ASSERTION]: 'a' != 'b'
```

Example use of `stackStartFunction` for truncating the exception's stacktrace:
```js
function suppressFrame() {
  assert.fail('a', 'b', undefined, '!==', suppressFrame);
}
suppressFrame();
// AssertionError [ERR_ASSERTION]: 'a' !== 'b'
//     at repl:1:1
//     at ContextifyScript.Script.runInThisContext (vm.js:44:33)
//     ...
```

## assert.ifError(value)
<!-- YAML
added: v0.1.97
-->
* `value` {any}

Throws `value` if `value` is truthy. This is useful when testing the `error`
argument in callbacks.

```js
const assert = require('assert');

assert.ifError(0);
// OK
assert.ifError(1);
// Throws 1
assert.ifError('error');
// Throws 'error'
assert.ifError(new Error());
// Throws Error
```

## assert.notDeepEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Tests for any deep inequality. Opposite of [`assert.deepEqual()`][].

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

assert.notDeepEqual(obj1, obj1);
// AssertionError: { a: { b: 1 } } notDeepEqual { a: { b: 1 } }

assert.notDeepEqual(obj1, obj2);
// OK, obj1 and obj2 are not deeply equal

assert.notDeepEqual(obj1, obj3);
// AssertionError: { a: { b: 1 } } notDeepEqual { a: { b: 1 } }

assert.notDeepEqual(obj1, obj4);
// OK, obj1 and obj2 are not deeply equal
```

If the values are deeply equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.notDeepStrictEqual(actual, expected[, message])
<!-- YAML
added: v1.2.0
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Tests for deep strict inequality. Opposite of [`assert.deepStrictEqual()`][].

```js
const assert = require('assert');

assert.notDeepEqual({ a: 1 }, { a: '1' });
// AssertionError: { a: 1 } notDeepEqual { a: '1' }

assert.notDeepStrictEqual({ a: 1 }, { a: '1' });
// OK
```

If the values are deeply and strictly equal, an `AssertionError` is thrown
with a `message` property set equal to the value of the `message` parameter. If
the `message` parameter is undefined, a default error message is assigned.

## assert.notEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Tests shallow, coercive inequality with the [Abstract Equality Comparison][]
( `!=` ).

```js
const assert = require('assert');

assert.notEqual(1, 2);
// OK

assert.notEqual(1, 1);
// AssertionError: 1 != 1

assert.notEqual(1, '1');
// AssertionError: 1 != '1'
```

If the values are equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned.

## assert.notStrictEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Tests strict inequality as determined by the [Strict Equality Comparison][]
( `!==` ).

```js
const assert = require('assert');

assert.notStrictEqual(1, 2);
// OK

assert.notStrictEqual(1, 1);
// AssertionError: 1 !== 1

assert.notStrictEqual(1, '1');
// OK
```

If the values are strictly equal, an `AssertionError` is thrown with a
`message` property set equal to the value of the `message` parameter. If the
`message` parameter is undefined, a default error message is assigned.

## assert.ok(value[, message])
<!-- YAML
added: v0.1.21
-->
* `value` {any}
* `message` {any}

Tests if `value` is truthy. It is equivalent to
`assert.equal(!!value, true, message)`.

If `value` is not truthy, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is `undefined`, a default error message is assigned.

```js
const assert = require('assert');

assert.ok(true);
// OK
assert.ok(1);
// OK
assert.ok(false);
// throws "AssertionError: false == true"
assert.ok(0);
// throws "AssertionError: 0 == true"
assert.ok(false, 'it\'s false');
// throws "AssertionError: it's false"
```

## assert.strictEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Tests strict equality as determined by the [Strict Equality Comparison][]
( `===` ).

```js
const assert = require('assert');

assert.strictEqual(1, 2);
// AssertionError: 1 === 2

assert.strictEqual(1, 1);
// OK

assert.strictEqual(1, '1');
// AssertionError: 1 === '1'
```

If the values are not strictly equal, an `AssertionError` is thrown with a
`message` property set equal to the value of the `message` parameter. If the
`message` parameter is undefined, a default error message is assigned.

## assert.throws(block[, error][, message])
<!-- YAML
added: v0.1.21
changes:
  - version: v4.2.0
    pr-url: https://github.com/nodejs/node/pull/3276
    description: The `error` parameter can now be an arrow function.
-->
* `block` {Function}
* `error` {RegExp|Function}
* `message` {any}

Expects the function `block` to throw an error.

If specified, `error` can be a constructor, [`RegExp`][], or validation
function.

If specified, `message` will be the message provided by the `AssertionError` if
the block fails to throw.

Validate instanceof using constructor:

```js
assert.throws(
  () => {
    throw new Error('Wrong value');
  },
  Error
);
```

Validate error message using [`RegExp`][]:

```js
assert.throws(
  () => {
    throw new Error('Wrong value');
  },
  /value/
);
```

Custom error validation:

```js
assert.throws(
  () => {
    throw new Error('Wrong value');
  },
  function(err) {
    if ((err instanceof Error) && /value/.test(err)) {
      return true;
    }
  },
  'unexpected error'
);
```

Note that `error` can not be a string. If a string is provided as the second
argument, then `error` is assumed to be omitted and the string will be used for
`message` instead. This can lead to easy-to-miss mistakes:

<!-- eslint-disable no-restricted-syntax -->
```js
// THIS IS A MISTAKE! DO NOT DO THIS!
assert.throws(myFunction, 'missing foo', 'did not throw with expected message');

// Do this instead.
assert.throws(myFunction, /missing foo/, 'did not throw with expected message');
```

## Caveats

For the following cases, consider using ES2015 [`Object.is()`][],
which uses the [SameValueZero][] comparison.

```js
const a = 0;
const b = -a;
assert.notStrictEqual(a, b);
// AssertionError: 0 !== -0
// Strict Equality Comparison doesn't distinguish between -0 and +0...
assert(!Object.is(a, b));
// but Object.is() does!

const str1 = 'foo';
const str2 = 'foo';
assert.strictEqual(str1 / 1, str2 / 1);
// AssertionError: NaN === NaN
// Strict Equality Comparison can't be used to check NaN...
assert(Object.is(str1 / 1, str2 / 1));
// but Object.is() can!
```

For more information, see
[MDN's guide on equality comparisons and sameness][mdn-equality-guide].

[`Error`]: errors.html#errors_class_error
[`Error.captureStackTrace`]: errors.html#errors_error_capturestacktrace_targetobject_constructoropt
[`Map`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Map
[`Object.is()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/is
[`RegExp`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Regular_Expressions
[`Set`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Set
[`TypeError`]: errors.html#errors_class_typeerror
[`assert.deepEqual()`]: #assert_assert_deepequal_actual_expected_message
[`assert.deepStrictEqual()`]: #assert_assert_deepstrictequal_actual_expected_message
[`assert.ok()`]: #assert_assert_ok_value_message
[`assert.throws()`]: #assert_assert_throws_block_error_message
[Abstract Equality Comparison]: https://tc39.github.io/ecma262/#sec-abstract-equality-comparison
[Object.prototype.toString()]: https://tc39.github.io/ecma262/#sec-object.prototype.tostring
[SameValueZero]: https://tc39.github.io/ecma262/#sec-samevaluezero
[Strict Equality Comparison]: https://tc39.github.io/ecma262/#sec-strict-equality-comparison
[caveats]: #assert_caveats
[enumerable "own" properties]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Enumerability_and_ownership_of_properties
[mdn-equality-guide]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Equality_comparisons_and_sameness
[prototype-spec]: https://tc39.github.io/ecma262/#sec-ordinary-object-internal-methods-and-internal-slots
