// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --for-of-optimization

// An iterator whose next method deopts the caller and throws (this emulates,
// among other things, Maglev's handling of unused exception handlers, which
// will lazy deopt the caller; however, the issue this is testing could have
// come from any kind of lazy deopt).
class DeoptAndThrowIterator {
  constructor(i, end) {
    this.i = i;
    this.end = end;
  }

  [Symbol.iterator]() {
    return this;
  }

  next() {
    if (this.i >= this.end) {
      %DeoptimizeFunction(looper);
      throw new Error();
    }
    return {value: this.i++, done: false};
  }
}

// A for-of where the called next method throws _and_ deopts the caller should
// still work.
function looper(it) {
  for (let obj of it) {
  }
}

%PrepareFunctionForOptimization(looper);
try { looper(new DeoptAndThrowIterator(0, 5)); } catch {}
try { looper(new DeoptAndThrowIterator(0, 5)); } catch {}
%OptimizeMaglevOnNextCall(looper);
try { looper(new DeoptAndThrowIterator(0, 5)); } catch {}
