// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function CycleDetection() {
  const arr = [
    {
      toLocaleString() {
        return [1, arr];
      }
    }
  ];
  assertSame('1,', arr.toLocaleString());
  assertSame('1,', arr.toLocaleString());
})();

(function ThrowsError(){
  function TestError() {}
  const arr = [];
  const obj = {
    toLocaleString(){
      throw new TestError();
    }
  };
  arr[0] = obj;
  assertThrows(() => arr.toLocaleString(), TestError);

  // Verifies cycle detection still works properly after thrown error.
  arr[0] = {
    toLocaleString() {
      return 1;
    }
  };
  assertSame('1', arr.toLocaleString());
})();

(function AccessThrowsError(){
  function TestError() {}
  const arr = [];
  const obj = {
    get toLocaleString(){
      throw new TestError();
    }
  };
  arr[0] = obj;
  assertThrows(() => arr.toLocaleString(), TestError);

  // Verifies cycle detection still works properly after thrown error.
  arr[0] = {
    toLocaleString() {
      return 1;
    }
  };
  assertSame('1', arr.toLocaleString());
})();

(function NotCallable(){
  const arr = [];
  const obj = {
    toLocaleString: 7
  }
  arr[0] = obj;
  assertThrows(() => arr.toLocaleString(), TypeError, 'number 7 is not a function');

  // Verifies cycle detection still works properly after thrown error.
  arr[0] = {
    toLocaleString() {
      return 1;
    }
  };
  assertSame('1', arr.toLocaleString());
})();
