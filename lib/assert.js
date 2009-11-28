var sys = require('sys');
var util = require('util');
var assert = exports;

assert.AssertionError = function (options) {
    this.name = "AssertionError";
    this.message = options.message;
    this.actual = options.actual;
    this.expected = options.expected;
    this.operator = options.operator;

    Error.captureStackTrace(this, fail);
};
sys.inherits(assert.AssertionError, Error);

// assert.AssertionError instanceof Error

// assert.AssertionError.prototype = Object.create(Error.prototype);

// At present only the three keys mentioned above are used and
// understood by the spec. Implementations or sub modules can pass
// other keys to the AssertionError's constructor - they will be
// ignored.

// 3. All of the following functions must throw an AssertionError
// when a corresponding condition is not met, with a message that
// may be undefined if not provided.  All assertion methods provide
// both the actual and expected values to the assertion error for
// display purposes.

function fail(actual, expected, message, operator) {
    throw new assert.AssertionError({
        message: message,
        actual: actual,
        expected: expected,
        operator: operator
    });
}

// 4. Pure assertion tests whether a value is truthy, as determined
// by !!guard.
// assert.ok(guard, message_opt);
// This statement is equivalent to assert.equal(true, guard,
// message_opt);. To test strictly for the value true, use
// assert.strictEqual(true, guard, message_opt);.

assert.ok = function (value, message) {
    if (!!!value)
        fail(value, true, message, "==");
};

// 5. The equality assertion tests shallow, coercive equality with
// ==.
// assert.equal(actual, expected, message_opt);

assert.equal = function (actual, expected, message) {
    if (actual != expected)
        fail(actual, expected, message, "==");
};


// 6. The non-equality assertion tests for whether two objects are not equal
// with != assert.notEqual(actual, expected, message_opt);

assert.notEqual = function (actual, expected, message) {
    if (actual == expected)
        fail(actual, expected, message, "!=");
};

// 7. The equivalence assertion tests a deep equality relation.
// assert.deepEqual(actual, expected, message_opt);

exports.deepEqual = function (actual, expected, message) {
    if (!deepEqual(actual, expected))
        fail(actual, expected, message, "deepEqual");
};

function deepEqual(actual, expected) {
    
    // 7.1. All identical values are equivalent, as determined by ===.
    if (actual === expected) {
        return true;

    // 7.2. If the expected value is a Date object, the actual value is
    // equivalent if it is also a Date object that refers to the same time.
    } else if (actual instanceof Date
            && expected instanceof Date) {
        return actual.toValue() === expected.toValue();

    // 7.3. Other pairs that do not both pass typeof value == "object",
    // equivalence is determined by ==.
    } else if (typeof actual != 'object'
            && typeof expected != 'object') {
        return actual == expected;

    // 7.4. For all other Object pairs, including Array objects, equivalence is
    // determined by having the same number of owned properties (as verified
    // with Object.prototype.hasOwnProperty.call), the same set of keys
    // (although not necessarily the same order), equivalent values for every
    // corresponding key, and an identical "prototype" property. Note: this
    // accounts for both named and indexed properties on Arrays.
    } else {
        return actual.prototype === expected.prototype && objEquiv(actual, expected);
    }
}

function objEquiv(a, b, stack) {
    return (
        !util.no(a) && !util.no(b) &&
        arrayEquiv(
            util.sort(util.object.keys(a)),
            util.sort(util.object.keys(b))
        ) &&
        util.object.keys(a).every(function (key) {
            return deepEqual(a[key], b[key], stack);
        })
    );
}

function arrayEquiv(a, b, stack) {
    return util.isArrayLike(b) &&
        a.length == b.length &&
        util.zip(a, b).every(util.apply(function (a, b) {
            return deepEqual(a, b, stack);
        }));
}

// 8. The non-equivalence assertion tests for any deep inequality.
// assert.notDeepEqual(actual, expected, message_opt);

exports.notDeepEqual = function (actual, expected, message) {
    if (deepEqual(actual, expected))
        fail(actual, expected, message, "notDeepEqual");
};

// 9. The strict equality assertion tests strict equality, as determined by ===.
// assert.strictEqual(actual, expected, message_opt);

assert.strictEqual = function (actual, expected, message) {
    if (actual !== expected)
        fail(actual, expected, message, "===");
};

// 10. The strict non-equality assertion tests for strict inequality, as determined by !==.
// assert.notStrictEqual(actual, expected, message_opt);

assert.notStrictEqual = function (actual, expected, message) {
    if (actual === expected)
        fail(actual, expected, message, "!==");
};

// 11. Expected to throw an error:
// assert.throws(block, Error_opt, message_opt);

assert["throws"] = function (block, Error, message) {
    var threw = false,
        exception = null;
        
    if (typeof Error == "string") {
        message = Error;
        Error = undefined;
    } else {
        Error = message;
        message = "";
    }

    try {
        block();
    } catch (e) {
        threw = true;
        exception = e;
    }
    
    if (!threw)
        fail("Expected exception" + (message ? ": " + message : ""));
};
