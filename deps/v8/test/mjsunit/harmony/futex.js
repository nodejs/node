// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-sharedarraybuffer

(function TestFailsWithNonSharedArray() {
  var ab = new ArrayBuffer(16);

  var i8a = new Int8Array(ab);
  var i16a = new Int16Array(ab);
  var i32a = new Int32Array(ab);
  var ui8a = new Uint8Array(ab);
  var ui8ca = new Uint8ClampedArray(ab);
  var ui16a = new Uint16Array(ab);
  var ui32a = new Uint32Array(ab);
  var f32a = new Float32Array(ab);
  var f64a = new Float64Array(ab);

  [i8a, i16a, i32a, ui8a, ui8ca, ui16a, ui32a, f32a, f64a].forEach(function(
      ta) {
    assertThrows(function() { Atomics.futexWait(ta, 0, 0); });
    assertThrows(function() { Atomics.futexWake(ta, 0, 1); });
    assertThrows(function() { Atomics.futexWakeOrRequeue(ta, 0, 1, 0, 0); });
  });
})();

(function TestFailsWithNonSharedInt32Array() {
  var sab = new SharedArrayBuffer(16);

  var i8a = new Int8Array(sab);
  var i16a = new Int16Array(sab);
  var ui8a = new Uint8Array(sab);
  var ui8ca = new Uint8ClampedArray(sab);
  var ui16a = new Uint16Array(sab);
  var ui32a = new Uint32Array(sab);
  var f32a = new Float32Array(sab);
  var f64a = new Float64Array(sab);

  [i8a, i16a, ui8a, ui8ca, ui16a, ui32a, f32a, f64a].forEach(function(
      ta) {
    assertThrows(function() { Atomics.futexWait(ta, 0, 0); });
    assertThrows(function() { Atomics.futexWake(ta, 0, 1); });
    assertThrows(function() { Atomics.futexWakeOrRequeue(ta, 0, 1, 0, 0); });
  });
})();

(function TestInvalidIndex() {
  var sab = new SharedArrayBuffer(16);
  var i32a = new Int32Array(sab);

  // Valid indexes are 0-3.
  [-1, 4, 100].forEach(function(invalidIndex) {
    assertThrows(function() {
      Atomics.futexWait(i32a, invalidIndex, 0);
    }, RangeError);
    assertThrows(function() {
      Atomics.futexWake(i32a, invalidIndex, 0);
    }, RangeError);
    var validIndex = 0;
    assertThrows(function() {
      Atomics.futexWakeOrRequeue(i32a, invalidIndex, 0, 0, validIndex);
    }, RangeError);
    assertThrows(function() {
      Atomics.futexWakeOrRequeue(i32a, validIndex, 0, 0, invalidIndex);
    }, RangeError);
  });

  i32a = new Int32Array(sab, 8);
  [-1, 2, 100].forEach(function(invalidIndex) {
    assertThrows(function() {
      Atomics.futexWait(i32a, invalidIndex, 0);
    }, RangeError);
    assertThrows(function() {
      Atomics.futexWake(i32a, invalidIndex, 0);
    }, RangeError);
    var validIndex = 0;
    assertThrows(function() {
      Atomics.futexWakeOrRequeue(i32a, invalidIndex, 0, 0, validIndex);
    }, RangeError);
    assertThrows(function() {
      Atomics.futexWakeOrRequeue(i32a, validIndex, 0, 0, invalidIndex);
    }, RangeError);
  });
})();

(function TestWaitTimeout() {
  var i32a = new Int32Array(new SharedArrayBuffer(16));
  var waitMs = 100;
  var startTime = new Date();
  assertEquals(Atomics.TIMEDOUT, Atomics.futexWait(i32a, 0, 0, waitMs));
  var endTime = new Date();
  assertTrue(endTime - startTime >= waitMs);
})();

(function TestWaitNotEqual() {
  var sab = new SharedArrayBuffer(16);
  var i32a = new Int32Array(sab);
  assertEquals(Atomics.NOTEQUAL, Atomics.futexWait(i32a, 0, 42));

  i32a = new Int32Array(sab, 8);
  i32a[0] = 1;
  assertEquals(Atomics.NOTEQUAL, Atomics.futexWait(i32a, 0, 0));
})();

(function TestWaitNegativeTimeout() {
  var i32a = new Int32Array(new SharedArrayBuffer(16));
  assertEquals(Atomics.TIMEDOUT, Atomics.futexWait(i32a, 0, 0, -1));
  assertEquals(Atomics.TIMEDOUT, Atomics.futexWait(i32a, 0, 0, -Infinity));
})();

//// WORKER ONLY TESTS

if (this.Worker) {

  var TestWaitWithTimeout = function(timeout) {
    var sab = new SharedArrayBuffer(16);
    var i32a = new Int32Array(sab);

    var workerScript =
      `onmessage = function(msg) {
         var i32a = new Int32Array(msg.sab, msg.offset);
         var result = Atomics.futexWait(i32a, 0, 0, ${timeout});
         postMessage(result);
       };`;

    var worker = new Worker(workerScript);
    worker.postMessage({sab: sab, offset: offset}, [sab]);

    // Spin until the worker is waiting on the futex.
    while (%AtomicsFutexNumWaitersForTesting(i32a, 0) != 1) {}

    Atomics.futexWake(i32a, 0, 1);
    assertEquals(Atomics.OK, worker.getMessage());
    worker.terminate();

    var worker2 = new Worker(workerScript);
    var offset = 8;
    var i32a2 = new Int32Array(sab, offset);
    worker2.postMessage({sab: sab, offset: offset}, [sab]);

    // Spin until the worker is waiting on the futex.
    while (%AtomicsFutexNumWaitersForTesting(i32a2, 0) != 1) {}
    Atomics.futexWake(i32a2, 0, 1);
    assertEquals(Atomics.OK, worker2.getMessage());
    worker2.terminate();

    // Futex should work when index and buffer views are different, but
    // the real address is the same.
    var worker3 = new Worker(workerScript);
    i32a2 = new Int32Array(sab, 4);
    worker3.postMessage({sab: sab, offset: 8}, [sab]);

    // Spin until the worker is waiting on the futex.
    while (%AtomicsFutexNumWaitersForTesting(i32a2, 1) != 1) {}
    Atomics.futexWake(i32a2, 1, 1);
    assertEquals(Atomics.OK, worker3.getMessage());
    worker3.terminate();
  };

  // Test various infinite timeouts
  TestWaitWithTimeout(undefined);
  TestWaitWithTimeout(NaN);
  TestWaitWithTimeout(Infinity);


  (function TestWakeMulti() {
    var sab = new SharedArrayBuffer(20);
    var i32a = new Int32Array(sab);

    // SAB values:
    // i32a[id], where id in range [0, 3]:
    //   0 => Worker |id| is still waiting on the futex
    //   1 => Worker |id| is not waiting on futex, but has not be reaped by the
    //        main thread.
    //   2 => Worker |id| has been reaped.
    //
    // i32a[4]:
    //   always 0. Each worker is waiting on this index.

    var workerScript =
      `onmessage = function(msg) {
         var id = msg.id;
         var i32a = new Int32Array(msg.sab);

         // Wait on i32a[4] (should be zero).
         var result = Atomics.futexWait(i32a, 4, 0);
         // Set i32a[id] to 1 to notify the main thread which workers were
         // woken up.
         Atomics.store(i32a, id, 1);
         postMessage(result);
       };`;

    var id;
    var workers = [];
    for (id = 0; id < 4; id++) {
      workers[id] = new Worker(workerScript);
      workers[id].postMessage({sab: sab, id: id}, [sab]);
    }

    // Spin until all workers are waiting on the futex.
    while (%AtomicsFutexNumWaitersForTesting(i32a, 4) != 4) {}

    // Wake up three waiters.
    assertEquals(3, Atomics.futexWake(i32a, 4, 3));

    var wokenCount = 0;
    var waitingId = 0 + 1 + 2 + 3;
    while (wokenCount < 3) {
      for (id = 0; id < 4; id++) {
        // Look for workers that have not yet been reaped. Set i32a[id] to 2
        // when they've been processed so we don't look at them again.
        if (Atomics.compareExchange(i32a, id, 1, 2) == 1) {
          assertEquals(Atomics.OK, workers[id].getMessage());
          workers[id].terminate();
          waitingId -= id;
          wokenCount++;
        }
      }
    }

    assertEquals(3, wokenCount);
    assertEquals(0, Atomics.load(i32a, waitingId));
    assertEquals(1, %AtomicsFutexNumWaitersForTesting(i32a, 4));

    // Finally wake the last waiter.
    assertEquals(1, Atomics.futexWake(i32a, 4, 1));
    assertEquals(Atomics.OK, workers[waitingId].getMessage());
    workers[waitingId].terminate();

    assertEquals(0, %AtomicsFutexNumWaitersForTesting(i32a, 4));

  })();

  (function TestWakeOrRequeue() {
    var sab = new SharedArrayBuffer(24);
    var i32a = new Int32Array(sab);

    // SAB values:
    // i32a[id], where id in range [0, 3]:
    //   0 => Worker |id| is still waiting on the futex
    //   1 => Worker |id| is not waiting on futex, but has not be reaped by the
    //        main thread.
    //   2 => Worker |id| has been reaped.
    //
    // i32a[4]:
    //   always 0. Each worker will initially wait on this index.
    //
    // i32a[5]:
    //   always 0. Requeued workers will wait on this index.

    var workerScript =
      `onmessage = function(msg) {
         var id = msg.id;
         var i32a = new Int32Array(msg.sab);

         var result = Atomics.futexWait(i32a, 4, 0, Infinity);
         Atomics.store(i32a, id, 1);
         postMessage(result);
       };`;

    var workers = [];
    for (id = 0; id < 4; id++) {
      workers[id] = new Worker(workerScript);
      workers[id].postMessage({sab: sab, id: id}, [sab]);
    }

    // Spin until all workers are waiting on the futex.
    while (%AtomicsFutexNumWaitersForTesting(i32a, 4) != 4) {}

    var index1 = 4;
    var index2 = 5;

    // If futexWakeOrRequeue is called with the incorrect value, it shouldn't
    // wake any waiters.
    assertEquals(Atomics.NOTEQUAL,
                 Atomics.futexWakeOrRequeue(i32a, index1, 1, 42, index2));

    assertEquals(4, %AtomicsFutexNumWaitersForTesting(i32a, index1));
    assertEquals(0, %AtomicsFutexNumWaitersForTesting(i32a, index2));

    // Now wake with the correct value.
    assertEquals(1, Atomics.futexWakeOrRequeue(i32a, index1, 1, 0, index2));

    // The workers that are still waiting should atomically be transferred to
    // the new index.
    assertEquals(3, %AtomicsFutexNumWaitersForTesting(i32a, index2));

    // The woken worker may not have been scheduled yet. Look for which thread
    // has set its i32a value to 1.
    var wokenCount = 0;
    while (wokenCount < 1) {
      for (id = 0; id < 4; id++) {
        if (Atomics.compareExchange(i32a, id, 1, 2) == 1) {
          wokenCount++;
        }
      }
    }

    assertEquals(0, %AtomicsFutexNumWaitersForTesting(i32a, index1));

    // Wake the remaining waiters.
    assertEquals(3, Atomics.futexWake(i32a, index2, 3));

    // As above, wait until the workers have been scheduled.
    wokenCount = 0;
    while (wokenCount < 3) {
      for (id = 0; id < 4; id++) {
        if (Atomics.compareExchange(i32a, id, 1, 2) == 1) {
          wokenCount++;
        }
      }
    }

    assertEquals(0, %AtomicsFutexNumWaitersForTesting(i32a, index1));
    assertEquals(0, %AtomicsFutexNumWaitersForTesting(i32a, index2));

    for (id = 0; id < 4; ++id) {
      assertEquals(Atomics.OK, workers[id].getMessage());
    }

    // Test futexWakeOrRequeue on offset typed array
    var offset = 16;
    sab = new SharedArrayBuffer(24);
    i32a = new Int32Array(sab);
    var i32a2 = new Int32Array(sab, offset);

    for (id = 0; id < 4; id++) {
      workers[id].postMessage({sab: sab, id: id}, [sab]);
    }

    while (%AtomicsFutexNumWaitersForTesting(i32a2, 0) != 4) { }

    index1 = 0;
    index2 = 1;
    assertEquals(4, %AtomicsFutexNumWaitersForTesting(i32a2, index1));
    assertEquals(0, %AtomicsFutexNumWaitersForTesting(i32a2, index2));

    assertEquals(2, Atomics.futexWakeOrRequeue(i32a2, index1, 2, 0, index2));
    assertEquals(2, %AtomicsFutexNumWaitersForTesting(i32a2, index2));
    assertEquals(0, %AtomicsFutexNumWaitersForTesting(i32a2, index1));

    assertEquals(2, Atomics.futexWake(i32a2, index2, 2));

    for (id = 0; id < 4; ++id) {
      assertEquals(Atomics.OK, workers[id].getMessage());
      workers[id].terminate();
    }

  })();

}
