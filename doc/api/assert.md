# Assert

<!--introduced_in=v0.10.0-->

> Stability: 2 - Stable

The `assert` module provides a simple set of assertion tests that can be used to
test invariants.

A `strict` and a `legacy` mode exist, while it is recommended to only use
[`strict mode`][].

For more information about the used equality comparisons see
[MDN's guide on equality comparisons and sameness][mdn-equality-guide].

## Strict mode
<!-- YAML
added: REPLACEME
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/REPLACEME
    description: Added error diffs to the strict mode
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/17002
    description: Added strict mode to the assert module.
-->

When using the `strict mode`, any `assert` function will use the equality used in
the strict function mode. So [`assert.deepEqual()`][] will, for example, work the
same as [`assert.deepStrictEqual()`][].

On top of that, error messages which involve objects produce an error diff
instead of displaying both objects. That is not the case for the legacy mode.

It can be accessed using:

```js
const assert = require('assert').strict;
```

Example error diff (the `expected`, `actual`, and `Lines skipped` will be on a
single row):

```js
const assert = require('assert').strict;

assert.deepEqual([[[1, 2, 3]], 4, 5], [[[1, 2, '3']], 4, 5]);
```

```diff
AssertionError [ERR_ASSERTION]: Input A expected to deepStrictEqual input B:
+ expected
- actual
... Lines skipped

  [
    [
...
      2,
-     3
+     '3'
    ],
...
    5
  ]
```

To deactivate the colors, use the `NODE_DISABLE_COLORS` environment variable.
Please note that this will also deactivate the colors in the REPL.

## Legacy mode

> Stability: 0 - Deprecated: Use strict mode instead.

When accessing `assert` directly instead of using the `strict` property, the
[Abstract Equality Comparison][] will be used for any function without a
"strict" in its name (e.g. [`assert.deepEqual()`][]).

It can be accessed using:

```js
const assert = require('assert');
```

It is recommended to use the [`strict mode`][] instead as the
[Abstract Equality Comparison][] can often have surprising results. Especially
in case of [`assert.deepEqual()`][] as the used comparison rules there are very
lax.

E.g.

```js
// WARNING: This does not throw an AssertionError!
assert.deepEqual(/a/gi, new Date());
```

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
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15001
    description: Error names and messages are now properly compared
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

**Strict mode**

An alias of [`assert.deepStrictEqual()`][].

**Legacy mode**

> Stability: 0 - Deprecated: Use [`assert.deepStrictEqual()`][] instead.

Tests for deep equality between the `actual` and `expected` parameters.
Primitive values are compared with the [Abstract Equality Comparison][]
( `==` ).

Only [enumerable "own" properties][] are considered. The
[`assert.deepEqual()`][] implementation does not test the
[`[[Prototype]]`][prototype-spec] of objects or enumerable own [`Symbol`][]
properties. For such checks, consider using [`assert.deepStrictEqual()`][]
instead. [`assert.deepEqual()`][] can have potentially surprising results. The
following example does not throw an `AssertionError` because the properties on
the [`RegExp`][] object are not enumerable:

```js
// WARNING: This does not throw an AssertionError!
assert.deepEqual(/a/gi, new Date());
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
parameter is undefined, a default error message is assigned. If the `message`
parameter is an instance of an [`Error`][] then it will be thrown instead of the
`AssertionError`.

## assert.deepStrictEqual(actual, expected[, message])
<!-- YAML
added: v1.2.0
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15169
    description: Enumerable symbol properties are now compared.
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15036
    description: NaN is now compared using the [SameValueZero][] comparison.
  - version: v8.5.0
    pr-url: https://github.com/nodejs/node/pull/15001
    description: Error names and messages are now properly compared
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

Tests for deep equality between the `actual` and `expected` parameters.
"Deep" equality means that the enumerable "own" properties of child objects
are recursively evaluated also by the following rules.

### Comparison details

* Primitive values are compared using the [SameValue Comparison][], used by
  [`Object.is()`][].
* [Type tags][Object.prototype.toString()] of objects should be the same.
* [`[[Prototype]]`][prototype-spec] of objects are compared using
  the [Strict Equality Comparison][].
* Only [enumerable "own" properties][] are considered.
* [`Error`][] names and messages are always compared, even if these are not
  enumerable properties.
* Enumerable own [`Symbol`][] properties are compared as well.
* [Object wrappers][] are compared both as objects and unwrapped values.
* Object properties are compared unordered.
* Map keys and Set items are compared unordered.
* Recursion stops when both sides differ or both sides encounter a circular
  reference.

```js
const assert = require('assert').strict;

assert.deepStrictEqual({ a: 1 }, { a: '1' });
// AssertionError: { a: 1 } deepStrictEqual { a: '1' }
// because 1 !== '1' using SameValue comparison

// The following objects don't have own properties
const date = new Date();
const object = {};
const fakeDate = {};
Object.setPrototypeOf(fakeDate, Date.prototype);

assert.deepStrictEqual(object, fakeDate);
// AssertionError: {} deepStrictEqual Date {}
// Different [[Prototype]]

assert.deepStrictEqual(date, fakeDate);
// AssertionError: 2017-03-11T14:25:31.849Z deepStrictEqual Date {}
// Different type tags

assert.deepStrictEqual(NaN, NaN);
// OK, because of the SameValue comparison

assert.deepStrictEqual(new Number(1), new Number(2));
// Fails because the wrapped number is unwrapped and compared as well.
assert.deepStrictEqual(new String('foo'), Object('foo'));
// OK because the object and the string are identical when unwrapped.

assert.deepStrictEqual(-0, -0);
// OK
assert.deepStrictEqual(0, -0);
// AssertionError: 0 deepStrictEqual -0

const symbol1 = Symbol();
const symbol2 = Symbol();
assert.deepStrictEqual({ [symbol1]: 1 }, { [symbol1]: 1 });
// OK, because it is the same symbol on both objects.
assert.deepStrictEqual({ [symbol1]: 1 }, { [symbol2]: 1 });
// Fails because symbol1 !== symbol2!
```

If the values are not equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned. If the `message`
parameter is an instance of an [`Error`][] then it will be thrown instead of the
`AssertionError`.

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

**Strict mode**

An alias of [`assert.strictEqual()`][].

**Legacy mode**

> Stability: 0 - Deprecated: Use [`assert.strictEqual()`][] instead.

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
parameter is undefined, a default error message is assigned. If the `message`
parameter is an instance of an [`Error`][] then it will be thrown instead of the
`AssertionError`.

## assert.fail([message])
## assert.fail(actual, expected[, message[, operator[, stackStartFunction]]])
<!-- YAML
added: v0.1.21
-->
* `actual` {any}
* `expected` {any}
* `message` {any} **Default:** `'Failed'`
* `operator` {string} **Default:** '!='
* `stackStartFunction` {Function} **Default:** `assert.fail`

Throws an `AssertionError`. If `message` is falsy, the error message is set as
the values of `actual` and `expected` separated by the provided `operator`. If
the `message` parameter is an instance of an [`Error`][] then it will be thrown
instead of the `AssertionError`. If just the two `actual` and `expected`
arguments are provided, `operator` will default to `'!='`. If `message` is
provided only it will be used as the error message, the other arguments will be
stored as properties on the thrown object. If `stackStartFunction` is provided,
all stack frames above that function will be removed from stacktrace (see
[`Error.captureStackTrace`]). If no arguments are given, the default message
`Failed` will be used.

```js
const assert = require('assert').strict;

assert.fail(1, 2, undefined, '>');
// AssertionError [ERR_ASSERTION]: 1 > 2

assert.fail(1, 2, 'fail');
// AssertionError [ERR_ASSERTION]: fail

assert.fail(1, 2, 'whoops', '>');
// AssertionError [ERR_ASSERTION]: whoops

assert.fail(1, 2, new TypeError('need array'));
// TypeError: need array
```

*Note*: In the last two cases `actual`, `expected`, and `operator` have no
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
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/18247
    description: Instead of throwing the original error it is now wrapped into
                 a AssertionError that contains the full stack trace.
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/18247
    description: Value may now only be `undefined` or `null`. Before any truthy
                 input was accepted.
-->
* `value` {any}

Throws `value` if `value` is not `undefined` or `null`. This is useful when
testing the `error` argument in callbacks.

```js
const assert = require('assert').strict;

assert.ifError(null);
// OK
assert.ifError(0);
// AssertionError [ERR_ASSERTION]: ifError got unwanted exception: 0
assert.ifError('error');
// AssertionError [ERR_ASSERTION]: ifError got unwanted exception: 'error'
assert.ifError(new Error());
// AssertionError [ERR_ASSERTION]: ifError got unwanted exception: Error
```

## assert.notDeepEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15001
    description: Error names and messages are now properly compared
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

**Strict mode**

An alias of [`assert.notDeepStrictEqual()`][].

**Legacy mode**

> Stability: 0 - Deprecated: Use [`assert.notDeepStrictEqual()`][] instead.

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
// OK: obj1 and obj2 are not deeply equal

assert.notDeepEqual(obj1, obj3);
// AssertionError: { a: { b: 1 } } notDeepEqual { a: { b: 1 } }

assert.notDeepEqual(obj1, obj4);
// OK: obj1 and obj4 are not deeply equal
```

If the values are deeply equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned. If the `message`
parameter is an instance of an [`Error`][] then it will be thrown instead of the
`AssertionError`.

## assert.notDeepStrictEqual(actual, expected[, message])
<!-- YAML
added: v1.2.0
changes:
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15398
    description: -0 and +0 are not considered equal anymore.
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15036
    description: NaN is now compared using the [SameValueZero][] comparison.
  - version: v9.0.0
    pr-url: https://github.com/nodejs/node/pull/15001
    description: Error names and messages are now properly compared
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

Tests for deep strict inequality. Opposite of [`assert.deepStrictEqual()`][].

```js
const assert = require('assert').strict;

assert.notDeepStrictEqual({ a: 1 }, { a: '1' });
// OK
```

If the values are deeply and strictly equal, an `AssertionError` is thrown with
a `message` property set equal to the value of the `message` parameter. If the
`message` parameter is undefined, a default error message is assigned. If the
`message` parameter is an instance of an [`Error`][] then it will be thrown
instead of the `AssertionError`.

## assert.notEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

**Strict mode**

An alias of [`assert.notStrictEqual()`][].

**Legacy mode**

> Stability: 0 - Deprecated: Use [`assert.notStrictEqual()`][] instead.

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

If the values are equal, an `AssertionError` is thrown with a `message` property
set equal to the value of the `message` parameter. If the `message` parameter is
undefined, a default error message is assigned. If the `message` parameter is an
instance of an [`Error`][] then it will be thrown instead of the
`AssertionError`.

## assert.notStrictEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/17003
    description: Used comparison changed from Strict Equality to `Object.is()`
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Tests strict inequality between the `actual` and `expected` parameters as
determined by the [SameValue Comparison][].

```js
const assert = require('assert').strict;

assert.notStrictEqual(1, 2);
// OK

assert.notStrictEqual(1, 1);
// AssertionError: 1 notStrictEqual 1

assert.notStrictEqual(1, '1');
// OK
```

If the values are strictly equal, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is undefined, a default error message is assigned. If the `message`
parameter is an instance of an [`Error`][] then it will be thrown instead of the
`AssertionError`.

## assert.ok(value[, message])
<!-- YAML
added: v0.1.21
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/17581
    description: assert.ok() will throw a `ERR_MISSING_ARGS` error.
                 Use assert.fail() instead.
-->
* `value` {any}
* `message` {any}

Tests if `value` is truthy. It is equivalent to
`assert.equal(!!value, true, message)`.

If `value` is not truthy, an `AssertionError` is thrown with a `message`
property set equal to the value of the `message` parameter. If the `message`
parameter is `undefined`, a default error message is assigned. If the `message`
parameter is an instance of an [`Error`][] then it will be thrown instead of the
`AssertionError`.

Be aware that in the `repl` the error message will be different to the one
thrown in a file! See below for further details.

```js
const assert = require('assert').strict;

assert.ok(true);
// OK
assert.ok(1);
// OK

assert.ok(false, 'it\'s false');
// throws "AssertionError: it's false"

// In the repl:
assert.ok(typeof 123 === 'string');
// throws:
// "AssertionError: false == true

// In a file (e.g. test.js):
assert.ok(typeof 123 === 'string');
// throws:
// "AssertionError: The expression evaluated to a falsy value:
//
//   assert.ok(typeof 123 === 'string')

assert.ok(false);
// throws:
// "AssertionError: The expression evaluated to a falsy value:
//
//   assert.ok(false)

assert.ok(0);
// throws:
// "AssertionError: The expression evaluated to a falsy value:
//
//   assert.ok(0)

// Using `assert()` works the same:
assert(0);
// throws:
// "AssertionError: The expression evaluated to a falsy value:
//
//   assert(0)
```

## assert.strictEqual(actual, expected[, message])
<!-- YAML
added: v0.1.21
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/17003
    description: Used comparison changed from Strict Equality to `Object.is()`
-->
* `actual` {any}
* `expected` {any}
* `message` {any}

Tests strict equality between the `actual` and `expected` parameters as
determined by the [SameValue Comparison][].

```js
const assert = require('assert').strict;

assert.strictEqual(1, 2);
// AssertionError: 1 strictEqual 2

assert.strictEqual(1, 1);
// OK

assert.strictEqual(1, '1');
// AssertionError: 1 strictEqual '1'
```

If the values are not strictly equal, an `AssertionError` is thrown with a
`message` property set equal to the value of the `message` parameter. If the
`message` parameter is undefined, a default error message is assigned. If the
`message` parameter is an instance of an [`Error`][] then it will be thrown
instead of the `AssertionError`.

## assert.throws(block[, error][, message])
<!-- YAML
added: v0.1.21
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/REPLACEME
    description: The `error` parameter can now be an object as well.
  - version: v4.2.0
    pr-url: https://github.com/nodejs/node/pull/3276
    description: The `error` parameter can now be an arrow function.
-->
* `block` {Function}
* `error` {RegExp|Function|Object}
* `message` {any}

Expects the function `block` to throw an error.

If specified, `error` can be a constructor, [`RegExp`][], a validation
function, or an object where each property will be tested for.

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

Using a regular expression runs `.toString` on the error object, and will
therefore also include the error name.

```js
assert.throws(
  () => {
    throw new Error('Wrong value');
  },
  /^Error: Wrong value$/
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

Custom error object / error instance:

```js
assert.throws(
  () => {
    const err = new TypeError('Wrong value');
    err.code = 404;
    throw err;
  },
  {
    name: 'TypeError',
    message: 'Wrong value'
    // Note that only properties on the error object will be tested!
  }
);
```

Note that `error` can not be a string. If a string is provided as the second
argument, then `error` is assumed to be omitted and the string will be used for
`message` instead. This can lead to easy-to-miss mistakes. Please read the
example below carefully if using a string as the second argument gets
considered:

<!-- eslint-disable no-restricted-syntax -->
```js
function throwingFirst() {
  throw new Error('First');
}
function throwingSecond() {
  throw new Error('Second');
}
function notThrowing() {}

// The second argument is a string and the input function threw an Error.
// In that case both cases do not throw as neither is going to try to
// match for the error message thrown by the input function!
assert.throws(throwingFirst, 'Second');
assert.throws(throwingSecond, 'Second');

// The string is only used (as message) in case the function does not throw:
assert.throws(notThrowing, 'Second');
// AssertionError [ERR_ASSERTION]: Missing expected exception: Second

// If it was intended to match for the error message do this instead:
assert.throws(throwingSecond, /Second$/);
// Does not throw because the error messages match.
assert.throws(throwingFirst, /Second$/);
// Throws a error:
// Error: First
//     at throwingFirst (repl:2:9)
```

Due to the confusing notation, it is recommended not to use a string as the
second argument. This might lead to difficult-to-spot errors.

[`Error.captureStackTrace`]: errors.html#errors_error_capturestacktrace_targetobject_constructoropt
[`Error`]: errors.html#errors_class_error
[`Map`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Map
[`Object.is()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/is
[`RegExp`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Regular_Expressions
[`Set`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Set
[`Symbol`]: https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Symbol
[`TypeError`]: errors.html#errors_class_typeerror
[`assert.deepEqual()`]: #assert_assert_deepequal_actual_expected_message
[`assert.deepStrictEqual()`]: #assert_assert_deepstrictequal_actual_expected_message
[`assert.notDeepStrictEqual()`]: #assert_assert_notdeepstrictequal_actual_expected_message
[`assert.notStrictEqual()`]: #assert_assert_notstrictequal_actual_expected_message
[`assert.ok()`]: #assert_assert_ok_value_message
[`assert.strictEqual()`]: #assert_assert_strictequal_actual_expected_message
[`assert.throws()`]: #assert_assert_throws_block_error_message
[`strict mode`]: #assert_strict_mode
[Abstract Equality Comparison]: https://tc39.github.io/ecma262/#sec-abstract-equality-comparison
[Object.prototype.toString()]: https://tc39.github.io/ecma262/#sec-object.prototype.tostring
[SameValue Comparison]: https://tc39.github.io/ecma262/#sec-samevalue
[Strict Equality Comparison]: https://tc39.github.io/ecma262/#sec-strict-equality-comparison
[enumerable "own" properties]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Enumerability_and_ownership_of_properties
[mdn-equality-guide]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Equality_comparisons_and_sameness
[prototype-spec]: https://tc39.github.io/ecma262/#sec-ordinary-object-internal-methods-and-internal-slots
[Object wrappers]: https://developer.mozilla.org/en-US/docs/Glossary/Primitive#Primitive_wrapper_objects_in_JavaScript
