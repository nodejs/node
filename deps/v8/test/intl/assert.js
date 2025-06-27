// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Some methods are taken from v8/test/mjsunit/mjsunit.js


function classOf(object) {
   // Argument must not be null or undefined.
   var string = Object.prototype.toString.call(object);
   // String has format [object <ClassName>].
   return string.substring(8, string.length - 1);
}

/**
 * Compares two objects for key/value equality.
 * Returns true if they are equal, false otherwise.
 */
function deepObjectEquals(a, b) {
  var aProps = Object.keys(a);
  aProps.sort();
  var bProps = Object.keys(b);
  bProps.sort();
  if (!deepEquals(aProps, bProps)) {
    return false;
  }
  for (var i = 0; i < aProps.length; i++) {
    if (!deepEquals(a[aProps[i]], b[aProps[i]])) {
      return false;
    }
  }
  return true;
}


/**
 * Compares two JavaScript values for type and value equality.
 * It checks internals of arrays and objects.
 */
function deepEquals(a, b) {
  if (a === b) {
    // Check for -0.
    if (a === 0) return (1 / a) === (1 / b);
    return true;
  }
  if (typeof a != typeof b) return false;
  if (typeof a == 'number') return isNaN(a) && isNaN(b);
  if (typeof a !== 'object' && typeof a !== 'function') return false;
  // Neither a nor b is primitive.
  var objectClass = classOf(a);
  if (objectClass !== classOf(b)) return false;
  if (objectClass === 'RegExp') {
    // For RegExp, just compare pattern and flags using its toString.
    return (a.toString() === b.toString());
  }
  // Functions are only identical to themselves.
  if (objectClass === 'Function') return false;
  if (objectClass === 'Array') {
    var elementCount = 0;
    if (a.length != b.length) {
      return false;
    }
    for (var i = 0; i < a.length; i++) {
      if (!deepEquals(a[i], b[i])) return false;
    }
    return true;
  }
  if (objectClass == 'String' || objectClass == 'Number' ||
      objectClass == 'Boolean' || objectClass == 'Date') {
    if (a.valueOf() !== b.valueOf()) return false;
  }
  return deepObjectEquals(a, b);
}

/**
 * Throws an exception containing the user_message (if any) and the values.
 */
function fail(expected, found, user_message = '') {
  // TODO(cira): Replace String with PrettyPrint for objects and arrays.
  var message = 'Failure' + (user_message ? ' (' + user_message + ')' : '') +
      ': expected <' + String(expected) + '>, found <' + String(found) + '>.';
  throw new Error(message);
}


/**
 * Throws if two variables have different types or values.
 */
function assertEquals(expected, found, user_message = '') {
  if (!deepEquals(expected, found)) {
    fail(expected, found, user_message);
  }
}


/**
 * Throws if value is false.
 */
function assertTrue(value, user_message = '') {
  assertEquals(true, value, user_message);
}


/**
 * Throws if value is true.
 */
function assertFalse(value, user_message = '') {
  assertEquals(false, value, user_message);
}


/**
 * Throws if value is null.
 */
function assertNotNull(value, user_message = '') {
  if (value === null) {
    fail("not null", value, user_message);
  }
}

/**
 * Throws if value is undefined.
 */
function assertNotUndefined(value, user_message = '') {
  if (value === undefined) {
    fail("not undefined", value, user_message);
  }
}


/**
 * Runs code() and asserts that it throws the specified exception.
 */
function assertThrows(code, type_opt, cause_opt) {
  try {
    if (typeof code == 'function') {
      code();
    } else {
      eval(code);
    }
  } catch (e) {
    if (typeof type_opt == 'function') {
      assertInstanceof(e, type_opt);
    }
    if (arguments.length >= 3) {
      assertEquals(cause_opt, e.message, 'thrown exception type mismatch');
    }
    // Success.
    return;
  }
  var expected = arguments.length >= 3 ? cause_opt :
      typeof type_opt == 'function' ? type_opt : 'any exception';
  fail(expected, 'no exception', 'expected thrown exception');
}


/**
 * Runs code() and asserts that it does now throw any exception.
 */
function assertDoesNotThrow(code, user_message = '') {
  try {
    if (typeof code == 'function') {
      code();
    } else {
      eval(code);
    }
  } catch (e) {
    fail("no expection", "exception: " + String(e), user_message);
  }
}


/**
 * Throws if obj is not of given type.
 */
function assertInstanceof(obj, type) {
  if (!(obj instanceof type)) {
    var actualTypeName = null;
    var actualConstructor = Object.getPrototypeOf(obj).constructor;
    if (typeof actualConstructor == "function") {
      actualTypeName = actualConstructor.name || String(actualConstructor);
    }
    throw new Error('Object <' + obj + '> is not an instance of <' +
                    (type.name || type) + '>' +
                    (actualTypeName ? ' but of < ' + actualTypeName + '>' : ''));
  }
}

/**
 * Split a BCP 47 language tag into locale and extension.
 */
function splitLanguageTag(tag) {
  // Search for the beginning of one or more extension tags, each of which
  // contains a singleton tag followed by one or more subtags. The equivalent
  // regexp is: /(-[0-9A-Za-z](-[0-9A-Za-z]{2,8})+)+$/. For example, in
  // 'de-DE-u-co-phonebk' the matched extension tags are '-u-co-phonebk'.
  //
  // The below is a mini-parser that reads backwards from the end of the string.

  function charCode(char) { return char.charCodeAt(0); }
  function isAlphaNumeric(code) {
    return (charCode("0") <= code && code <= charCode("9")) ||
           (charCode("A") <= code && code <= charCode("Z")) ||
           (charCode("a") <= code && code <= charCode("z"));
  }

  const MATCH_SUBTAG = 0;
  const MATCH_SINGLETON_OR_SUBTAG = 1;
  let state = MATCH_SUBTAG;

  const MINIMUM_TAG_LENGTH = 2;
  const MAXIMUM_TAG_LENGTH = 8;
  let currentTagLength = 0;

  // -1 signifies failure, a non-negative integer is the start index of the
  // extension tag.
  let extensionTagStartIndex = -1;

  for (let i = tag.length - 1; i >= 0; i--) {
    const currentCharCode = tag.charCodeAt(i);
    if (currentCharCode == charCode("-")) {
      if (state == MATCH_SINGLETON_OR_SUBTAG && currentTagLength == 1) {
        // Found the singleton tag, the match succeeded.
        // Save the matched index, and reset the state. After this point, we
        // definitely have a match, but we may still find another extension tag
        // sequence.
        extensionTagStartIndex = i;
        state = MATCH_SUBTAG;
        currentTagLength = 0;
      } else if (MINIMUM_TAG_LENGTH <= currentTagLength &&
                currentTagLength <= MAXIMUM_TAG_LENGTH) {
        // Found a valid subtag.
        state = MATCH_SINGLETON_OR_SUBTAG;
        currentTagLength = 0;
      } else {
        // Invalid subtag (too short or too long).
        break;
      }
    } else if (isAlphaNumeric(currentCharCode)) {
      // An alphanumeric character is potentially part of a tag.
      currentTagLength++;
    } else {
      // Any other character is invalid.
      break;
    }
  }

  if (extensionTagStartIndex != -1) {
    return { locale: tag.substring(0, extensionTagStartIndex),
             extension: tag.substring(extensionTagStartIndex) };
  }

  return { locale: tag, extension: '' };
}


/**
 * Throw if |parent| is not a more general language tag of |child|, nor |child|
 * itself, per BCP 47 rules.
 */
function assertLanguageTag(child, parent) {
  var childSplit = splitLanguageTag(child);
  var parentSplit = splitLanguageTag(parent);

  // Do not compare extensions at this moment, as %GetDefaultICULocale()
  // doesn't always output something we support.
  if (childSplit.locale !== parentSplit.locale &&
      !childSplit.locale.startsWith(parentSplit.locale + '-')) {
    fail(child, parent, 'language tag comparison');
  }
}

function assertArrayEquals(expected, found, name_opt) {
  var start = "";
  if (name_opt) {
    start = name_opt + " - ";
  }
  assertEquals(expected.length, found.length, start + "array length");
  if (expected.length === found.length) {
    for (var i = 0; i < expected.length; ++i) {
      assertEquals(expected[i], found[i],
                   start + "array element at index " + i);
    }
  }
}
