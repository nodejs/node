# Assert

    Stability: 3 - Locked

This module is used so that Node.js can test itself. It can be accessed with
`require('assert')`. However, it is recommended that a userland assertion
library be used instead.

## assert.fail(actual, expected, message, operator)

Throws an exception that displays the values for `actual` and `expected`
separated by the provided operator.

## assert(value[, message]), assert.ok(value[, message])

Tests if value is truthy. It is equivalent to
`assert.equal(true, !!value, message)`.

## assert.equal(actual, expected[, message])

Tests shallow, coercive equality with the equal comparison operator ( `==` ).

## assert.notEqual(actual, expected[, message])

Tests shallow, coercive inequality with the not equal comparison operator
( `!=` ).

## assert.deepEqual(actual, expected[, message])

Tests for deep equality. Primitive values are compared with the equal
comparison operator ( `==` ).

This only considers enumerable properties. It does not test object prototypes,
attached symbols, or non-enumerable properties. This can lead to some
potentially surprising results. For example, this does not throw an
`AssertionError` because the properties on the `Error` object are
non-enumerable:

    // WARNING: This does not throw an AssertionError!
    assert.deepEqual(Error('a'), Error('b'));

## assert.notDeepEqual(actual, expected[, message])

Tests for any deep inequality. Opposite of `assert.deepEqual`.

## assert.strictEqual(actual, expected[, message])

Tests strict equality as determined by the strict equality operator ( `===` ).

## assert.notStrictEqual(actual, expected[, message])

Tests strict inequality as determined by the strict not equal operator
( `!==` ).

## assert.deepStrictEqual(actual, expected[, message])

Tests for deep equality. Primitive values are compared with the strict equality
operator ( `===` ).

## assert.notDeepStrictEqual(actual, expected[, message])

Tests for deep inequality. Opposite of `assert.deepStrictEqual`.

## assert.throws(block[, error][, message])

Expects `block` to throw an error. `error` can be a constructor, `RegExp`, or
validation function.

Validate instanceof using constructor:

    assert.throws(
      function() {
        throw new Error("Wrong value");
      },
      Error
    );

Validate error message using RegExp:

    assert.throws(
      function() {
        throw new Error("Wrong value");
      },
      /value/
    );

Custom error validation:

    assert.throws(
      function() {
        throw new Error("Wrong value");
      },
      function(err) {
        if ( (err instanceof Error) && /value/.test(err) ) {
          return true;
        }
      },
      "unexpected error"
    );

## assert.doesNotThrow(block[, error][, message])

Expects `block` not to throw an error. See [assert.throws()](#assert_assert_throws_block_error_message) for more details.

If `block` throws an error and if it is of a different type from `error`, the
thrown error will get propagated back to the caller. The following call will
throw the `TypeError`, since we're not matching the error types in the
assertion.

    assert.doesNotThrow(
      function() {
        throw new TypeError("Wrong value");
      },
      SyntaxError
    );

In case `error` matches with the error thrown by `block`, an `AssertionError`
is thrown instead.

    assert.doesNotThrow(
      function() {
        throw new TypeError("Wrong value");
      },
      TypeError
    );

## assert.ifError(value)

Throws `value` if `value` is truthy. This is useful when testing the `error`
argument in callbacks.
