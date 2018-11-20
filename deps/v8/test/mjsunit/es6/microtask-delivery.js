// Copyright 2012 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax

var ordering = [];
function reset() {
  ordering = [];
}

function assertArrayValues(expected, actual) {
  assertEquals(expected.length, actual.length);
  for (var i = 0; i < expected.length; i++) {
    assertEquals(expected[i], actual[i]);
  }
}

function assertOrdering(expected) {
  %RunMicrotasks();
  assertArrayValues(expected, ordering);
}

function newPromise(id, fn) {
  var r;
  var t = 1;
  var promise = new Promise(function(resolve) {
    r = resolve;
    if (fn) fn();
  });

  var next = promise.then(function(value) {
    ordering.push('p' + id);
    return value;
  });

  return {
    resolve: r,
    then: function(fn) {
      next = next.then(function(value) {
        ordering.push('p' + id + ':' + t++);
        return fn ? fn(value) : value;
      });

      return this;
    }
  };
}

function newObserver(id, fn, obj) {
  var observer = {
    value: 1,
    recordCounts: []
  };

  Object.observe(observer, function(records) {
    ordering.push('o' + id);
    observer.recordCounts.push(records.length);
    if (fn) fn();
  });

  return observer;
}


(function PromiseThens() {
  reset();

  var p1 = newPromise(1).then();
  var p2 = newPromise(2).then();

  p1.resolve();
  p2.resolve();

  assertOrdering(['p1', 'p2', 'p1:1', 'p2:1']);
})();


(function ObserversBatch() {
  reset();

  var p1 = newPromise(1);
  var p2 = newPromise(2);
  var p3 = newPromise(3);

  var ob1 = newObserver(1);
  var ob2 = newObserver(2, function() {
    ob3.value++;
    p3.resolve();
    ob1.value++;
  });
  var ob3 = newObserver(3);

  p1.resolve();
  ob1.value++;
  p2.resolve();
  ob2.value++;

  assertOrdering(['p1', 'o1', 'o2', 'p2', 'o1', 'o3', 'p3']);
  assertArrayValues([1, 1], ob1.recordCounts);
  assertArrayValues([1], ob2.recordCounts);
  assertArrayValues([1], ob3.recordCounts);
})();


(function ObserversGetAllRecords() {
  reset();

  var p1 = newPromise(1);
  var p2 = newPromise(2);
  var ob1 = newObserver(1, function() {
    ob2.value++;
  });
  var ob2 = newObserver(2);

  p1.resolve();
  ob1.value++;
  p2.resolve();
  ob2.value++;

  assertOrdering(['p1', 'o1', 'o2', 'p2']);
  assertArrayValues([1], ob1.recordCounts);
  assertArrayValues([2], ob2.recordCounts);
})();


(function NewObserverDeliveryGetsNewMicrotask() {
  reset();

  var p1 = newPromise(1);
  var p2 = newPromise(2);
  var ob1 = newObserver(1);
  var ob2 = newObserver(2, function() {
    ob1.value++;
  });

  p1.resolve();
  ob1.value++;
  p2.resolve();
  ob2.value++;

  assertOrdering(['p1', 'o1', 'o2', 'p2', 'o1']);
  assertArrayValues([1, 1], ob1.recordCounts);
  assertArrayValues([1], ob2.recordCounts);
})();
