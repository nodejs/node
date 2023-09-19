// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const iterable = {
  [Symbol.iterator]: () => ({
    next: () => ({
      done: false,
      get value() {
        assertUnreachable()
        print('"value" getter is called');
        return 42;
      }
    })
  })
};

[,] = iterable;
