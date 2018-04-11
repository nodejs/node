// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extensions to mjsunit and other test harnesses added between harness and
// fuzzing code.

try {
  // Scope for utility functions.
  (function() {
    // Same as in mjsunit.js.
    function classOf(object) {
      // Argument must not be null or undefined.
      var string = Object.prototype.toString.call(object);
      // String has format [object <ClassName>].
      return string.substring(8, string.length - 1);
    }

    // Override prettyPrinted with a version that also recusively prints object
    // properties (with a depth of 3).
    let origPrettyPrinted = this.prettyPrinted;
    this.prettyPrinted = function prettyPrinted(value, depth=3) {
      if (depth == 0) {
        return "...";
      }
      switch (typeof value) {
        case "object":
          if (value === null) return "null";
          var objectClass = classOf(value);
          switch (objectClass) {
            case "Object":
              var name = value.constructor.name;
              if (!name)
                name = "Object";
              return name + "{" + Object.keys(value).map(function(key, index) {
                return (
                    prettyPrinted(key, depth - 1) +
                    ": " +
                    prettyPrinted(value[key], depth - 1)
                );
              }).join(",")  + "}";
          }
      }
      // Fall through to original version for all other types.
      return origPrettyPrinted(value);
    }

    // We're not interested in stack traces.
    this.MjsUnitAssertionError = function MjsUnitAssertionError(message) {}
    MjsUnitAssertionError.prototype.toString = function () { return ""; };

    // Do more printing in assertions for more correctness coverage.
    this.failWithMessage = function failWithMessage(message) {
      print(prettyPrinted(message))
    }

    this.fail = function fail(expectedText, found, name_opt) {
      print(prettyPrinted(found));
    }

    this.assertSame = function assertSame(expected, found, name_opt) {
      print(prettyPrinted(found));
    }

    this.assertNotSame = function assertNotSame(expected, found, name_opt) {
      print(prettyPrinted(found));
    }

    this.assertEquals = function assertEquals(expected, found, name_opt) {
      print(prettyPrinted(found));
    }

    this.assertNotEquals = function assertNotEquals(expected, found, name_opt) {
      print(prettyPrinted(found));
    }

    this.assertNull = function assertNull(value, name_opt) {
      print(prettyPrinted(value));
    }

    this.assertNotNull = function assertNotNull(value, name_opt) {
      print(prettyPrinted(value));
    }

    // Suppress optimization status as it leads to false positives.
    this.assertUnoptimized = function assertUnoptimized() {}

    this.assertOptimized = function assertOptimized() {}

    this.isNeverOptimize = function isNeverOptimize() {}

    this.isAlwaysOptimize = function isAlwaysOptimize() {}

    this.isInterpreted = function isInterpreted() {}

    this.isOptimized = function isOptimized() {}

    this.isTurboFanned = function isTurboFanned() {}
  })();
} catch(e) { }
