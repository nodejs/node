/*
 * testharness-helpers contains various useful extensions to testharness.js to
 * allow them to be used across multiple tests before they have been
 * upstreamed. This file is intended to be usable from both document and worker
 * environments, so code should for example not rely on the DOM.
 */

// Asserts that two objects |actual| and |expected| are weakly equal under the
// following definition:
//
// |a| and |b| are weakly equal if any of the following are true:
//   1. If |a| is not an 'object', and |a| === |b|.
//   2. If |a| is an 'object', and all of the following are true:
//     2.1 |a.p| is weakly equal to |b.p| for all own properties |p| of |a|.
//     2.2 Every own property of |b| is an own property of |a|.
//
// This is a replacement for the version of assert_object_equals() in
// testharness.js. The latter doesn't handle own properties correctly. I.e. if
// |a.p| is not an own property, it still requires that |b.p| be an own
// property.
//
// Note that |actual| must not contain cyclic references.
self.assert_object_equals = function(actual, expected, description) {
  var object_stack = [];

  function _is_equal(actual, expected, prefix) {
    if (typeof actual !== 'object') {
      assert_equals(actual, expected, prefix);
      return;
    }
    assert_equals(typeof expected, 'object', prefix);
    assert_equals(object_stack.indexOf(actual), -1,
                  prefix + ' must not contain cyclic references.');

    object_stack.push(actual);

    Object.getOwnPropertyNames(expected).forEach(function(property) {
        assert_own_property(actual, property, prefix);
        _is_equal(actual[property], expected[property],
                  prefix + '.' + property);
      });
    Object.getOwnPropertyNames(actual).forEach(function(property) {
        assert_own_property(expected, property, prefix);
      });

    object_stack.pop();
  }

  function _brand(object) {
    return Object.prototype.toString.call(object).match(/^\[object (.*)\]$/)[1];
  }

  _is_equal(actual, expected,
            (description ? description + ': ' : '') + _brand(expected));
};

// Equivalent to assert_in_array, but uses a weaker equivalence relation
// (assert_object_equals) than '==='.
function assert_object_in_array(actual, expected_array, description) {
  assert_true(expected_array.some(function(element) {
      try {
        assert_object_equals(actual, element);
        return true;
      } catch (e) {
        return false;
      }
    }), description);
}

// Assert that the two arrays |actual| and |expected| contain the same set of
// elements as determined by assert_object_equals. The order is not significant.
//
// |expected| is assumed to not contain any duplicates as determined by
// assert_object_equals().
function assert_array_equivalent(actual, expected, description) {
  assert_true(Array.isArray(actual), description);
  assert_equals(actual.length, expected.length, description);
  expected.forEach(function(expected_element) {
      // assert_in_array treats the first argument as being 'actual', and the
      // second as being 'expected array'. We are switching them around because
      // we want to be resilient against the |actual| array containing
      // duplicates.
      assert_object_in_array(expected_element, actual, description);
    });
}

// Asserts that two arrays |actual| and |expected| contain the same set of
// elements as determined by assert_object_equals(). The corresponding elements
// must occupy corresponding indices in their respective arrays.
function assert_array_objects_equals(actual, expected, description) {
  assert_true(Array.isArray(actual), description);
  assert_equals(actual.length, expected.length, description);
  actual.forEach(function(value, index) {
      assert_object_equals(value, expected[index],
                           description + ' : object[' + index + ']');
    });
}

// Asserts that |object| that is an instance of some interface has the attribute
// |attribute_name| following the conditions specified by WebIDL, but it's
// acceptable that the attribute |attribute_name| is an own property of the
// object because we're in the middle of moving the attribute to a prototype
// chain.  Once we complete the transition to prototype chains,
// assert_will_be_idl_attribute must be replaced with assert_idl_attribute
// defined in testharness.js.
//
// FIXME: Remove assert_will_be_idl_attribute once we complete the transition
// of moving the DOM attributes to prototype chains.  (http://crbug.com/43394)
function assert_will_be_idl_attribute(object, attribute_name, description) {
  assert_equals(typeof object, "object", description);

  assert_true("hasOwnProperty" in object, description);

  // Do not test if |attribute_name| is not an own property because
  // |attribute_name| is in the middle of the transition to a prototype
  // chain.  (http://crbug.com/43394)

  assert_true(attribute_name in object, description);
}

// Stringifies a DOM object.  This function stringifies not only own properties
// but also DOM attributes which are on a prototype chain.  Note that
// JSON.stringify only stringifies own properties.
function stringifyDOMObject(object)
{
    function deepCopy(src) {
        if (typeof src != "object")
            return src;
        var dst = Array.isArray(src) ? [] : {};
        for (var property in src) {
            dst[property] = deepCopy(src[property]);
        }
        return dst;
    }
    return JSON.stringify(deepCopy(object));
}
