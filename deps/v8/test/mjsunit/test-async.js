// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Used for async tests. See definition below for more documentation.
var testAsync;

(function () {  // Scope for utility functions.
  /**
   * This is to be used through the testAsync helper function defined
   * below.
   *
   * This requires the --allow-natives-syntax flag to allow calling
   * runtime functions.
   *
   * There must be at least one assertion in an async test. A test
   * with no assertions will fail.
   *
   * @example
   * testAsync(assert => {
   *   assert.plan(1) // There should be one assertion in this test.
   *   Promise.resolve(1)
   *    .then(val => assert.equals(1, val),
   *          assert.unreachable);
   * })
   */
  class AsyncAssertion {
    constructor(test, name) {
      this.expectedAsserts_ = -1;
      this.actualAsserts_ = 0;
      this.test_ = test;
      this.name_ = name || '';
    }

    /**
     * Sets the number of expected asserts in the test. The test fails
     * if the number of asserts computed after running the test is not
     * equal to this specified value.
     * @param {number} expectedAsserts
     */
    plan(expectedAsserts) {
      this.expectedAsserts_ = expectedAsserts;
    }

    fail(expectedText, found) {
      let message = formatFailureText(expectedText, found);
      message += "\nin test:" + this.name_
      message += "\n" + Function.prototype.toString.apply(this.test_);
      %AbortJS(message);
    }

    equals(expected, found, name_opt) {
      this.actualAsserts_++;
      if (!deepEquals(expected, found)) {
        this.fail(prettyPrinted(expected), found, name_opt);
      }
    }

    unreachable() {
      let message = "Failure: unreachable in test: " + this.name_;
      message += "\n" + Function.prototype.toString.apply(this.test_);
      %AbortJS(message);
    }

    unexpectedRejection(details) {
      return (error) => {
        let message =
            "Failure: unexpected Promise rejection in test: " + this.name_;
        if (details) message += "\n    @" + details;
        if (error instanceof Error) {
          message += "\n" + String(error.stack);
        } else {
          message += "\n" + String(error);
        }
        message += "\n\n" + Function.prototype.toString.apply(this.test_);
        %AbortJS(message);
      };
    }

    drainMicrotasks() {
      %PerformMicrotaskCheckpoint();
    }

    done_() {
      if (this.expectedAsserts_ === -1) {
        let message = "Please call t.plan(count) to initialize test harness " +
            "with correct assert count (Note: count > 0)";
        %AbortJS(message);
      }

      if (this.expectedAsserts_ !== this.actualAsserts_) {
        let message = "Expected asserts: " + this.expectedAsserts_;
        message += ", Actual asserts: " + this.actualAsserts_;
        message += "\nin test: " + this.name_;
        message += "\n" + Function.prototype.toString.apply(this.test_);
        %AbortJS(message);
      }
    }
  }

  /** This is used to test async functions and promises.
   * @param {testCallback} test - test function
   * @param {string} [name] - optional name of the test
   *
   *
   * @callback testCallback
   * @param {AsyncAssertion} assert
   */
  testAsync = function(test, name) {
    let assert = new AsyncAssertion(test, name);
    test(assert);
    %PerformMicrotaskCheckpoint();
    assert.done_();
  }
})();
