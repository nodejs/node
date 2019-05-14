// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Trigger an infinite loop through RegExp.prototype[@@match], which results
// in unbounded growth of the results array.

// Limit the number of iterations to avoid OOM while still triggering large
// object space allocation.
const min_ptr_size = 4;
const max_regular_heap_object_size = 507136;
const num_iterations = max_regular_heap_object_size / min_ptr_size;

let i = 0;

const re = /foo.bar/;
const RegExpPrototypeExec = RegExp.prototype.exec;
re.exec = (str) => {
  return (i++ < num_iterations) ? RegExpPrototypeExec.call(re, str) : null;
};
re.__defineGetter__("global", () => true);  // Triggers infinite loop.

"foo*bar".match(re);  // Should not crash.
