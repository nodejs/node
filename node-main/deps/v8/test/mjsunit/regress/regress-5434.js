// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ensure that we have the correct number of accesses to exec in split, and
// that exec is called at the correct point in time.

var lastIndexHasBeenSet = false;
var countOfExecGets = 0;

// Force the slow path and make sure the created splitter object has our
// overwritten exec method (@@split does not call exec on the original regexp
// but on a newly-created splitter which is guaranteed to be sticky).
class ObservableExecRegExp extends RegExp {
  constructor(pattern, flags) {
    super(pattern, flags);
    this.lastIndex = 42;

    const re = this;
    Object.defineProperty(this, "exec", {
      get: function() {
        // Ensure exec is first accessed after lastIndex has been reset to
        // satisfy the spec.
        assertTrue(re.lastIndex != 42);
        countOfExecGets++;
        return RegExp.prototype.exec;
      }
    });
  }
}



var re = new ObservableExecRegExp(/x/);

assertEquals(42, re.lastIndex);
assertEquals(0, countOfExecGets);

var result = "axbxc".split(re);

assertEquals(5, countOfExecGets);
assertEquals(["a", "b", "c"], result);
