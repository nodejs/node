// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const MIN_DICTIONARY_INDEX = 8192;

function ArrayTests() {
  (function ToStringThrows() {
    function TestError() {}

    let callCount = 0;
    const toStringThrows = {
      toString() {
        callCount++;
        throw new TestError;
      }
    };
    const a = [toStringThrows];
    assertThrows(() => a.join(), TestError);
    assertSame(1, callCount);

    // Verifies cycle detection still works properly after thrown error.
    a[0] = 1;
    a[1] = 2;
    assertSame('1,2', a.join());
  })();

  (function ArrayLengthIncreased() {
    let callCount = 0;
    const a = [
      {
        toString() {
          callCount++;
          a.push(2);
          return '1';
        }
      }
    ];
    assertSame('1', a.join());
    assertSame(1, callCount);
    assertSame('1,2', a.join());
  })();

  (function ArrayLengthDecreased() {
    let callCount = 0;
    const a = [
      {
        toString() {
          callCount++;
          a.pop();
          return '1';
        }
      },
      '2'
    ];
    assertSame('1,', a.join());
    assertSame(1, callCount);
    assertSame('1', a.join());
  })();

  (function ElementsKindChangedToHoley() {
    let callCount = 0;
    const a = [
      {
        toString() {
          callCount++;
          a.length = 4;
          a[1] = 777;
          a[2] = 7.7;
          return '1';
        }
      },
      2,
      3
    ];
    assertSame('1,777,7.7', a.join());
    assertSame(1, callCount);
    assertSame('1,777,7.7,', a.join());
  })();

  (function ElementsKindChangedToHoleyThroughDeletion() {
    let callCount = 0;
    const a = [
      {
        toString() {
          callCount++;
          delete a[1];
          a[2] = 7.7;
          return '1';
        }
      },
      2,
      3
    ];
    assertSame('1,,7.7', a.join());
    assertSame(1, callCount);
    assertSame('1,,7.7', a.join());
  })();

  (function NumberDictionaryChanged() {
    let callCount = 0;
    const a = [];
    a[MIN_DICTIONARY_INDEX - 1] = {
      toString() {
        callCount++;
        a[MIN_DICTIONARY_INDEX] = '2';
        return '1';
      }
    };
    a[MIN_DICTIONARY_INDEX] = 'NOPE';
    assertTrue(%HasDictionaryElements(a));
    assertSame('12', a.join(''));
    assertSame(1, callCount);
    assertSame('12', a.join(''));
  })();

  (function NumberDictionaryLengthChange() {
    let callCount = 0;
    const a = [];
    a[MIN_DICTIONARY_INDEX - 1] = {
      toString() {
        callCount++;
        a.length = MIN_DICTIONARY_INDEX;
        return '1';
      }
    };
    a[MIN_DICTIONARY_INDEX] = '2';
    assertTrue(%HasDictionaryElements(a));
    assertSame('1', a.join(''));
    assertSame(1, callCount);
    assertSame('1', a.join(''));
  })();
}

(function NonArrayCycleDetection() {
  const a = {
    length: 3,
    toString() { return Array.prototype.join.call(this); }
  };
  a[0] = '1';
  a[1] = a;
  a[2] = '3';
  assertSame("1,,3", Array.prototype.join.call(a));
});

ArrayTests();

%SetForceSlowPath(true);

ArrayTests();
