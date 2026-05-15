// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --for-of-optimization

// Ensure 'value' is not retrieved when 'done' is true.
(function TestValueNotAccessedWhenDoneIsTrue() {
  let valueAccessed = false;
  let count = 0;
  const resultObject = {
    get done() {
      return true;
    },
    get value() {
      valueAccessed = true;
      return 'should not be accessed';
    }
  };

  const iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          return resultObject;
        }
      };
    }
  };

  function runLoop() {
    for (const x of iterable) {
      count++;
    }
  }

  runLoop();
  assertEquals(0, count);
  assertFalse(
      valueAccessed,
      'The \'value\' property was accessed even though \'done\' was true!');
})();

// Builtin handles exceptions in the 'done' getter.
(function TestExceptionInDoneGetter() {
  let count = 0;
  const iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          return {
            get done() {
              throw new Error('DoneGetterError');
            },
            value: 42
          };
        }
      };
    }
  };

  function runLoop() {
    try {
      for (const x of iterable) {
        count++;
      }
    } catch (e) {
      return e.message;
    }
    return 'no error';
  }

  assertEquals('DoneGetterError', runLoop());
  assertEquals(0, count);
})();

// Builtin handles exceptions in the 'value' getter.
(function TestExceptionInValueGetter() {
  let count = 0;
  const iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          return {
            done: false,
            get value() {
              throw new Error('ValueGetterError');
            }
          };
        }
      };
    }
  };

  function runLoop() {
    try {
      for (const x of iterable) {
        count++;
      }
    } catch (e) {
      return e.message;
    }
    return 'no error';
  }

  assertEquals('ValueGetterError', runLoop());
  assertEquals(0, count);
})();
