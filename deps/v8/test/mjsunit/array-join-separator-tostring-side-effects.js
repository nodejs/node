// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const MIN_DICTIONARY_INDEX = 8192;

(function ToStringThrows() {
  function TestError() {}

  let callCount = 0;
  const a = [1, 2];
  assertThrows(() => a.join({
    toString() {
      callCount++;
      throw new TestError;
    }
  }), TestError);
  assertSame(1, callCount);

  // Verifies cycle detection still works properly after thrown error.
  assertSame('1,2', a.join());
})();

(function RecursiveJoinCall() {
  const a = [1,2,3];
  let callCount = 0;
  const sep = {
    toString() {
      callCount++;
      return a.join('-');
    }
  };
  assertSame('11-2-321-2-33', a.join(sep));
  assertSame(1, callCount);

  // Verify cycle detection works properly after nested call
  assertSame('1,2,3', a.join());
})();


(function ArrayLengthIncreased() {
  const a = [1,2,3];
  let callCount = 0;
  assertSame('1,2,3', a.join({
    toString() {
      callCount++;
      a.push(4);
      return ',';
    }
  }));
  assertSame(1, callCount);
  assertSame('1,2,3,4', a.join());
})();

(function ArrayLengthDecreased() {
  const a = [1,2,3];
  let callCount = 0;
  assertSame('1,2,', a.join({
    toString() {
      callCount++;
      a.pop();
      return ',';
    }
  }));
  assertSame(1, callCount);
  assertSame('1,2', a.join());
})();

(function ArrayEmptied() {
  const a = [1,2,3];
  let callCount = 0;
  assertSame(',,', a.join({
    toString() {
      callCount++;
      a.length = 0;
      return ',';
    }
  }));
  assertSame(1, callCount);
})();

(function NumberDictionaryEmptied() {
  const a = [];
  a[0] = 1;
  a[MIN_DICTIONARY_INDEX] = 2;
  assertTrue(%HasDictionaryElements(a));

  let callCount = 0;
  assertSame('-'.repeat(MIN_DICTIONARY_INDEX), a.join({
    toString() {
      callCount++;
      a.length = 0;
      return '-';
    }
  }));
  assertSame(1, callCount);
})();

(function NumberDictionaryEmptiedEmptySeparator() {
  const a = [];
  a[0] = 1;
  a[MIN_DICTIONARY_INDEX] = 2;
  assertTrue(%HasDictionaryElements(a));

  let callCount = 0;
  assertSame(''.repeat(MIN_DICTIONARY_INDEX), a.join({
    toString() {
      callCount++;
      a.length = 0;
      return '';
    }
  }));
  assertSame(1, callCount);
})();

(function ElementsKindSmiToDoubles() {
  const a = [1,2,3];
  let callCount = 0;
  assertTrue(%HasSmiElements(a));
  assertSame('1.5,2,3', a.join({
    toString() {
      callCount++;
      a[0] = 1.5;
      assertTrue(%HasDoubleElements(a));
      return ',';
    }
  }));
  assertSame(1, callCount);
  assertSame('1.5,2,3', a.join());
})();

(function ElementsKindDoublesToObjects() {
  const a = [1.5, 2.5, 3.5];
  let callCount = 0;
  assertTrue(%HasDoubleElements(a));
  assertSame('one,2.5,3.5', a.join({
    toString() {
      callCount++;
      a[0] = 'one';
      assertTrue(%HasObjectElements(a));
      return ',';
    }
  }));
  assertSame(1, callCount);
  assertSame('one,2.5,3.5', a.join());
})();

(function ArrayIsNoLongerFast() {
  const a = [1,2,3];
  let callCount = 0;
  assertSame('666,2,3', a.join({
    toString() {
      callCount++;
      Object.defineProperty(a, '0', {
        get(){ return 666; }
      });
      return ',';
    }
  }));
  assertSame(1, callCount);
  assertSame('666,2,3', a.join());
})();

(function ArrayPrototypeUnset() {
  const a = [1,2];
  a.length = 3;
  let callCount = 0;
  assertSame('1,2,4', a.join({
    toString() {
      callCount++;
      a.__proto__ = { '2': 4 };
      return ',';
    }
  }));
  assertSame(1, callCount);
  a.__proto__ = Array.prototype;
  assertSame('1,2,', a.join());
})();

(function ArrayPrototypeIsNoLongerFast() {
  const a = [1,2,3];
  let callCount = 0;
  assertSame('1,2,777', a.join({
    toString() {
      callCount++;
      a.pop();
      Object.defineProperty(Array.prototype, '2', {
        get(){ return 777; }
      });
      return ',';
    }
  }));
  assertSame(1, callCount);
  assertSame('1,2', a.join());
})();
